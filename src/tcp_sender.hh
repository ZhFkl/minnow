#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>
#include <queue>
class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ),current_RTO_ms_( initial_RTO_ms ),rexmit_queue_()
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }
  bool syn_sent_ = false;
  bool fin_sent_ = false; 
  uint64_t next_seq_{0};  // 下一个要发送的序列号
  uint64_t acked_seq_{0};// 已确认的序列号
  uint64_t in_flight_{0};// 未确认但已发送的字节数
  uint64_t mss_{1000};

  int raw_window_size_{-1};
  uint16_t window_size_{1};
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t current_RTO_ms_;
  uint64_t time_elapsed_{0};
  bool timer_running_{false};
  std::queue<TCPSenderMessage> rexmit_queue_;
  uint64_t consecutive_rexmit_cnt_{0};// 连续重传计数


};
