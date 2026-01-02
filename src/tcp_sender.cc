#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const { return in_flight_; }

uint64_t TCPSender::consecutive_retransmissions() const { return consecutive_rexmit_cnt_; }

void TCPSender::push( const TransmitFunction& transmit )
{
  // 1. 检查流错误，发送 RST 并立即返回 (不重传 RST)
  if (input_.writer().has_error()) {
    TCPSenderMessage rst_msg;
    rst_msg.seqno = Wrap32::wrap(next_seq_, isn_);
    rst_msg.SYN = false;
    rst_msg.FIN = false;
    rst_msg.RST = true;
    transmit(rst_msg);
    return;
  }

  // 2. 防止下溢：计算可用窗口
  uint64_t current_window_size = (window_size_ == 0) ? 1 : window_size_;
  uint64_t available_space = 0;
  if (current_window_size > in_flight_) {
      available_space = current_window_size - in_flight_;
  }

  // 3. 填充窗口循环
  while (available_space > 0) {
    TCPSenderMessage msg;
    msg.SYN = false;
    msg.FIN = false;
    msg.RST = false;

    // 处理 SYN
    if (!syn_sent_) {
      msg.SYN = true;
      syn_sent_ = true;
      available_space--;
    }

    // 计算 Payload (注意这里 MSS 的使用，如果 TCPConfig 可用建议替换 mss_)
    uint64_t payload_size = std::min({mss_, available_space, input_.reader().bytes_buffered()});
    msg.payload = input_.reader().peek().substr(0, payload_size);
    input_.reader().pop(payload_size);
    available_space -= payload_size;

    // 处理 FIN (只有在空间允许且流结束时)
    if (!fin_sent_ && input_.reader().is_finished() && available_space > 0) {
      msg.FIN = true;
      fin_sent_ = true;
      available_space--;
    }

    msg.seqno = Wrap32::wrap(next_seq_, isn_);

    // 如果没有任何东西发送 (长度为0且不是RST)，退出循环
    if (msg.sequence_length() == 0) {
      break;
    }

    // 发送并更新状态
    transmit(msg);
    next_seq_ += msg.sequence_length();
    in_flight_ += msg.sequence_length();
    rexmit_queue_.push(msg);

    // 只有在定时器未运行时才启动
    if (!timer_running_) {
      timer_running_ = true;
      time_elapsed_ = 0;
    }

    // 如果发送了 FIN 或者窗口满了，停止
    if (fin_sent_ || available_space == 0) {
      break;
    }
  }
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.RST) {
      input_.writer().set_error();
      // 这里不需要清空队列或者重置定时器，因为连接已经断了
      // 但清空也是安全的防御性编程
      return; 
  }

  // 更新窗口大小 (即使没有 ACK 新数据，窗口也可能更新)
  raw_window_size_ = msg.window_size;
  window_size_ = (msg.window_size == 0) ? 1 : msg.window_size;

  if (msg.ackno.has_value()) {
    uint64_t abs_ackno = msg.ackno.value().unwrap(isn_, acked_seq_);

    // 检查 ACK 合法性：不能小于已确认的，也不能大于已发送的
    if (abs_ackno > next_seq_ || abs_ackno <= acked_seq_) {
      return;
    }

    // 更新 in_flight 和 acked_seq
    in_flight_ -= (abs_ackno - acked_seq_);
    acked_seq_ = abs_ackno;

    // 清理重传队列
    while (!rexmit_queue_.empty()) {
      const auto &front_msg = rexmit_queue_.front();
      // 这里的 checkpoint 用 acked_seq_ 更自然
      uint64_t msg_end = front_msg.seqno.unwrap(isn_, acked_seq_) + front_msg.sequence_length();
      
      if (msg_end <= acked_seq_) {
        rexmit_queue_.pop();
      } else {
        break;
      }
    }

    // RFC 6298: 有新数据被确认时，重置 RTO
    current_RTO_ms_ = initial_RTO_ms_;
    consecutive_rexmit_cnt_ = 0;
    time_elapsed_ = 0;

    // 如果还有未确认数据，重启定时器(通过置0已完成)；如果没有，关闭定时器
    timer_running_ = !rexmit_queue_.empty();
  }
}


TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.SYN = false;
  msg.FIN = false;
  if(input_.writer().has_error()){
    msg.RST = true;
  }else{
    msg.RST = false;
  }
  msg.payload = "";
  msg.seqno = Wrap32::wrap(next_seq_, isn_);
  return msg;
}


void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if(timer_running_){
    time_elapsed_ +=  ms_since_last_tick;
  }

  // check if timeout
  if(timer_running_ && time_elapsed_ >= current_RTO_ms_ && !rexmit_queue_.empty()){
    // timeout , retransmit the oldest segment
    const auto& msg = rexmit_queue_.front();
    transmit(msg);

    // exponential backoff
    if(raw_window_size_ != 0 ){
      current_RTO_ms_ *= 2;
      consecutive_rexmit_cnt_ += 1;
    }

    // reset timer
    time_elapsed_ = 0;
  }
  // if the rexmit queue is empty, stop the timer
  if(rexmit_queue_.empty()){
    timer_running_ = false;
    time_elapsed_ = 0;
  }


  (void)transmit;
}


// seq to write the function 
// receive push tick