#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// How many sequence numbers are outstanding?
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return in_flight_;
  debug( "unimplemented sequence_numbers_in_flight() called" );
  return {};
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_rexmit_cnt_;
  debug( "unimplemented consecutive_retransmissions() called" );
  return {};
}

void TCPSender::push( const TransmitFunction& transmit )
{
  auto available_space = static_cast<uint64_t>(window_size_) - in_flight_;
  // send SYN if not sent
  if(available_space == 0){
    debug("window is full, cannot send data now");
    return ;
  }
  while(available_space > 0 ){

    TCPSenderMessage msg;
    msg.SYN = false;
    msg.FIN = false;

    if(input_.writer().has_error()){
      msg.RST = true;
    }else{
      msg.RST = false;
    }

    if(!syn_sent_ && available_space > 0){
      msg.SYN = true;
      syn_sent_ = true;
      available_space -= 1;
    }


    uint64_t max_data_len = std::min({
        mss_,
        available_space,
        input_.reader().bytes_buffered()
    });

    msg.payload = input_.reader().peek().substr(0, max_data_len);
    input_.reader().pop(max_data_len);
    available_space -= max_data_len;
    // if the input stream is not ended and we should use a while loop to 
    // push more data to the segment

    if(!fin_sent_ && input_.reader().is_finished() 
      && available_space > 0 && input_.writer().is_closed()){
        msg.FIN = true;
        fin_sent_ = true;
        available_space -= 1;
    }


    msg.seqno = Wrap32::wrap(next_seq_, isn_);

    if(msg.sequence_length() == 0 && !msg.RST){
      // nothing to send
      return ;
    }
    transmit(msg);
    next_seq_ += msg.sequence_length();
    in_flight_ += msg.sequence_length();
    rexmit_queue_.push(msg);
    // start rto timer if not running
    if(!timer_running_){
      timer_running_ = true;
      time_elapsed_ = 0;
    }

    if(fin_sent_ || available_space == 0 || msg.RST){
      break;
    }


  }



  debug( "unimplemented push() called" );
  (void)transmit;
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

  debug( "unimplemented make_empty_message() called" );
  return {};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.RST) {
        input_.writer().set_error();
        while (!rexmit_queue_.empty()) {
            rexmit_queue_.pop();
        }
        timer_running_ = false;
        time_elapsed_ = 0;
        return; 
  }
  if(!input_.writer().has_error()){
    raw_window_size_ = msg.window_size;
    window_size_ = (msg.window_size == 0) ? 1 : msg.window_size;
    if(msg.ackno.has_value()){
      //which means the ack is valid then we get the ackno and rto

      uint64_t abs_ackno = msg.ackno.value().unwrap(isn_, acked_seq_);

      if(abs_ackno <= acked_seq_ || abs_ackno > next_seq_){
        //invalid ackno 
        return;
      }
      in_flight_ = in_flight_ - (abs_ackno - acked_seq_);
      acked_seq_ = abs_ackno;
      while(!rexmit_queue_.empty()){
        const auto& front_msg = rexmit_queue_.front();
        uint64_t msg_end = front_msg.seqno.unwrap(isn_,acked_seq_) + front_msg.sequence_length();
        
        // check if the front msg is acked
        if(msg_end <= acked_seq_){
          rexmit_queue_.pop();
        }else{
          break;
        }
      }

      // reset rto timer and consecutive rexmit cnt
      consecutive_rexmit_cnt_ = 0;
      // reset RTO timer
      current_RTO_ms_ = initial_RTO_ms_;

      if(rexmit_queue_.empty()){
        timer_running_ = false;
        time_elapsed_ = 0;
      }else{
        time_elapsed_ = 0;
      }

    } 
  }

  debug( "unimplemented receive() called" );
  (void)msg;
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


  debug( "unimplemented tick({}, ...) called", ms_since_last_tick );
  (void)transmit;
}


// seq to write the function 
// receive push tick