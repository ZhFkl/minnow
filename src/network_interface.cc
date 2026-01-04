#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( InternetDatagram dgram, const Address& next_hop )
{
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // check if we have the MAC address in ARP table
    auto arp_iter = arp_table_.find(next_hop_ip);
    if(arp_iter != arp_table_.end()){
      EthernetFrame frame;
      frame.header.dst = arp_iter->second.mac;
      frame.header.src = ethernet_address_;
      frame.header.type = EthernetHeader::TYPE_IPv4;

      // 
      Serializer serializer;
      dgram.serialize(serializer);
      frame.payload = serializer.finish();
      transmit(frame);
      return;
    }

        // [修改点]：如果 ARP 请求已存在且超时（>5000ms），清空旧的等待数据包
    const auto timer_iter = arp_request_timers_.find(next_hop_ip);
    if (timer_iter != arp_request_timers_.end()) {
      if (current_time_ - timer_iter->second > 5000) {
        waiting_datagrams_.erase(next_hop_ip); // 丢弃旧包
        // 这里不删除 timer_iter，让下面的逻辑去更新时间戳即可
      }
    }
    // if not in arp table, check if we have sent ARP request recently
    waiting_datagrams_[next_hop_ip].push_back(dgram);


    if(timer_iter == arp_request_timers_.end()||(current_time_ - timer_iter->second > 5000)){
      ARPMessage arp_request;
      arp_request.opcode = ARPMessage::OPCODE_REQUEST;
      arp_request.sender_ethernet_address = ethernet_address_;
      arp_request.sender_ip_address = ip_address_.ipv4_numeric();
      arp_request.target_ethernet_address = {0,0,0,0,0,0};
      arp_request.target_ip_address = next_hop_ip;

      EthernetFrame frame;
      frame.header.dst = ETHERNET_BROADCAST;
      frame.header.src = ethernet_address_;
      frame.header.type = EthernetHeader::TYPE_ARP;
      //this time the arp request not send yet

      Serializer serializer;
      arp_request.serialize(serializer);
      frame.payload = serializer.finish();
      transmit(frame);
      arp_request_timers_[next_hop_ip] = current_time_;
      return;
    }




  debug( "unimplemented send_datagram called" );
  (void)dgram;
  (void)next_hop;
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  // check if the frame is destined to this interface
  if(frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST){
    return;
  }
  // handle IPv4 frame
  if(frame.header.type == EthernetHeader::TYPE_IPv4){
    Parser parser( frame.payload );
    InternetDatagram dgram;
    dgram.parse(parser);
    if( !parser.has_error()){
      datagrams_received_.push(dgram);
    }
    return; 
  }

  // handle ARP frame
  if(frame.header.type == EthernetHeader::TYPE_ARP){
    ARPMessage arp_msg;
    Parser parser(frame.payload);
    arp_msg.parse(parser);
    if(parser.has_error() ){
      return ;
    }


    

    const uint32_t sender_ip = arp_msg.sender_ip_address;
    const EthernetAddress sender_mac = arp_msg.sender_ethernet_address;
    arp_table_[sender_ip] = {sender_mac, current_time_ + 30000};

    // if ARP request for us, send ARP reply
    if(arp_msg.opcode == ARPMessage:: OPCODE_REQUEST && arp_msg.target_ip_address == ip_address_
       .ipv4_numeric()){
        ARPMessage arp_reply;
        arp_reply.opcode = ARPMessage::OPCODE_REPLY;
        arp_reply.sender_ethernet_address = ethernet_address_;
        arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
        arp_reply.target_ethernet_address = sender_mac;
        arp_reply.target_ip_address = sender_ip;
        EthernetFrame reply_frame;
        reply_frame.header.dst = sender_mac;
        reply_frame.header.src = ethernet_address_;
        reply_frame.header.type = EthernetHeader::TYPE_ARP;

        Serializer serializer;
        arp_reply.serialize(serializer);
        reply_frame.payload = serializer.finish();
        transmit(reply_frame);
    }



    // [重点修改这里]：处理等待队列
    auto waiting_iter = waiting_datagrams_.find(sender_ip);
    if(waiting_iter != waiting_datagrams_.end()){
      
      // 检查发出请求的时间
      bool should_send = true;
      const auto timer_iter = arp_request_timers_.find(sender_ip);
      if (timer_iter != arp_request_timers_.end()) {
        // 如果当前时间距离上次请求超过 5000ms，说明这个 Reply 来得太晚了
        // 原本等待的那些数据包已经“因为超时而失效”了，不应该发送
        if (current_time_ - timer_iter->second > 5000) {
          should_send = false;
        }
      }

      // 只有在没超时的情况下才发送
      if (should_send) {
        for(const auto& dgram : waiting_iter->second){
          EthernetFrame out_frame;
          out_frame.header.dst = sender_mac;
          out_frame.header.src = ethernet_address_;
          out_frame.header.type = EthernetHeader::TYPE_IPv4;

          Serializer serializer;
          dgram.serialize(serializer);
          out_frame.payload = serializer.finish();
          transmit(out_frame);
        }
      }

      // 无论是否发送，都将其从等待队列中移除
      // 发送了 -> 任务完成
      // 超时了 -> 丢弃包
      waiting_datagrams_.erase(waiting_iter);
      
      arp_request_timers_.erase(sender_ip);


    }
  }
  
  debug( "unimplemented recv_frame called" );
  (void)frame;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  current_time_ += ms_since_last_tick;


  // 
  for(auto it = arp_table_.begin(); it != arp_table_.end();){
    if(it->second.expiration_time <= current_time_){
      it = arp_table_.erase(it);
    }else{
      ++it;
    }
  }

  

  debug( "unimplemented tick({}) called", ms_since_last_tick );
}
