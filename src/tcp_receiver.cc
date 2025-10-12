#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  //first set the isn

  if(message.RST){  
    rst_ = true;
    checkpoint = 0;
    isn_.reset();
    reassembler_.set_error();
    return ;
  }
  if(message.SYN){
    isn_ = message.seqno;
  }
  if(!isn_.has_value()){
    return;
  }
  if(message.FIN){
    is_finish = true;
  }
  string data = message.payload;
  //because of the isn take over a bit so we need to -1 to get the data offset 
  uint64_t index;
  
  if(message.SYN){
    index = message.seqno.unwrap(isn_.value(),checkpoint);
  }else{
    index = message.seqno.unwrap(isn_.value(),checkpoint) -1;
  }
  reassembler_.insert(index,data,is_finish);
  checkpoint = index + data.size();
  return ;

  debug( "unimplemented receive() called" );
  (void)message;
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage tsm_;
  if(reassembler_.reader().has_error() || rst_){
      tsm_.RST = true;
  }
  if(isn_.has_value()){
    uint32_t size = reassembler_.writer().bytes_pushed() + 1 ;
    if(is_finish && reassembler_.count_bytes_pending() == 0){
      size++;
    }
    Wrap32 ackno = isn_.value() + size;
    tsm_.ackno = ackno;
  }else{
    tsm_.ackno = nullopt;
  }
  uint64_t window_size_ = reassembler_.writer().available_capacity();
  if(window_size_ > UINT16_MAX){
    tsm_.window_size =UINT16_MAX;
  }else{
    tsm_.window_size = static_cast<uint16_t> ( window_size_);
  }
  return tsm_;

  debug( "unimplemented send() called" );
  return {};
}
