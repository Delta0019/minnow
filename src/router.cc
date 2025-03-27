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
  route_table_.insert_node( route_prefix, prefix_length, next_hop, interface_num );
  return;
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( const auto& route_interface : interfaces_ ) {
    auto& queue = route_interface->datagrams_received();
    while ( not queue.empty() ) {
      InternetDatagram dgram = queue.front();
      queue.pop();
      if ( dgram.header.ttl <= 1 ) {
        continue;
      }
      dgram.header.ttl -= 1;

      uint32_t dgram_dst_ip = dgram.header.dst;
      pair<optional<Address>, size_t> tmp = longest_prefix_match( dgram_dst_ip );
      shared_ptr<NetworkInterface> target_interface = interface( tmp.second );
      Address next_hop = tmp.first ? tmp.first.value() : Address::from_ipv4_numeric( dgram_dst_ip );
      target_interface->send_datagram( dgram, next_hop );
    }
  }
}

pair<optional<Address>, size_t> Router::longest_prefix_match( const uint32_t dst_ip )
{
  return route_table_.longest_prefix_match( dst_ip );
}
