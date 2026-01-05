#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  route_table_.push_back({route_prefix,prefix_length,next_hop,interface_num});
  debug( "unimplemented add_route() called" );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for(auto& current_interface: interfaces_){
    auto& queue = current_interface->datagrams_received();

    while(!queue.empty()){
      InternetDatagram dgram = queue.front();
      queue.pop();
      auto best_match = route_table_.end();
      int max_prefix_len = -1;
      uint32_t dst_ip = dgram.header.dst;
      

      for(auto it = route_table_.begin();it != route_table_.end();++it){

        uint32_t mask = (it->prefix_length == 0) ? 0 : (0xFFFFFFFF << (32 -it->prefix_length));

        if((dst_ip & mask) == (it->route_prefix & mask)){
          if(static_cast<int>(it->prefix_length) > max_prefix_len){
            max_prefix_len = it->prefix_length;
            best_match = it;
          }
        }
      }

      // zhuan fa
      if(best_match != route_table_.end() && dgram.header.ttl > 1){
        dgram.header.ttl--;
        dgram.header.compute_checksum();
        Address next_hop = best_match->next_hop.has_value() 
                           ? best_match->next_hop.value() 
                           : Address::from_ipv4_numeric( dst_ip );
        interface( best_match->interface_num )->send_datagram( dgram, next_hop );
      }else{

      }
    }
  }
  debug( "unimplemented route() called" );
}
