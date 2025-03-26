#include <iomanip>
#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"
#include "sstream"

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

void NetworkInterface::send_InternetDatagram( const InternetDatagram& dgram, const uint32_t dst_ip )
{
  cout << "----Send IPv4 dgram----" << endl;

  EthernetFrame frame;
  frame.header.src = ethernet_address_;
  frame.header.dst = arp_table_[dst_ip].first;
  frame.header.type = EthernetHeader::TYPE_IPv4;

  Serializer serializer;
  dgram.serialize( serializer );
  frame.payload = serializer.finish();

  transmit( frame );
}

void NetworkInterface::send_ARPRequest( const uint32_t dst_ip )
{
  cout << "----Send ARP request, dst_ip: " << dst_ip << endl;

  ARPMessage arp_request;
  arp_request.opcode = ARPMessage::OPCODE_REQUEST;

  arp_request.sender_ethernet_address = ethernet_address_;
  arp_request.sender_ip_address = ip_address_.ipv4_numeric();

  arp_request.target_ethernet_address = {};
  arp_request.target_ip_address = dst_ip;

  EthernetFrame frame;
  frame.header.src = ethernet_address_;
  frame.header.dst = ETHERNET_BROADCAST;
  frame.header.type = EthernetHeader::TYPE_ARP;

  Serializer serializer;
  arp_request.serialize( serializer );
  frame.payload = serializer.finish();

  cout << "ARP request: " << arp_request.to_string() << endl;
  transmit( frame );
  arp_table_waiting_[dst_ip] = 5000;
}

void NetworkInterface::send_ARPReply( const uint32_t dst_ip )
{
  ARPMessage arp_reply;
  arp_reply.opcode = ARPMessage::OPCODE_REPLY;

  arp_reply.sender_ethernet_address = ethernet_address_;
  arp_reply.sender_ip_address = ip_address_.ipv4_numeric();

  arp_reply.target_ethernet_address = arp_table_[dst_ip].first;
  arp_reply.target_ip_address = dst_ip;

  EthernetFrame frame;
  frame.header.src = ethernet_address_;
  frame.header.dst = arp_table_[dst_ip].first;
  frame.header.type = EthernetHeader::TYPE_ARP;

  Serializer serializer;
  arp_reply.serialize( serializer );
  frame.payload = serializer.finish();

  cout << "----ARP reply: " << arp_reply.to_string() << endl;
  transmit( frame );
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t raw_dst_IP_address = next_hop.ipv4_numeric();

  auto it = arp_table_.find( raw_dst_IP_address );

  if ( it == arp_table_.end() ) {
    // ARP request is not in the ARP waiting table
    if ( arp_table_waiting_.find( raw_dst_IP_address ) == arp_table_waiting_.end() ) {
      send_ARPRequest( raw_dst_IP_address );
    }
    dgram_table_[next_hop.ipv4_numeric()].push_back( pair<InternetDatagram, int32_t> { dgram, ARP_TTL_wait } );

  } else {
    send_InternetDatagram( dgram, raw_dst_IP_address );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ ) {
    return;
  }

  Parser parser { frame.payload };
  if ( parser.has_error() ) {
    debug( "Error parsing Ethernet frame" );
    return;
  }

  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_IPv4: {
      InternetDatagram dgram;
      dgram.parse( parser );
      datagrams_received_.push( dgram );
      break;
    }

    case EthernetHeader::TYPE_ARP: {

      ARPMessage arp_message;
      arp_message.parse( parser );
      cout << "Recv ARP: " << arp_message.to_string() << endl;

      uint32_t msg_src_ip = arp_message.sender_ip_address;
      EthernetAddress msg_src_mac = arp_message.sender_ethernet_address;
      cout << "Get IP and ARP: " << msg_src_ip << " ; " << to_string( msg_src_mac ) << endl;
      // Add to ARP table from both ARP request and ARP reply
      arp_table_[msg_src_ip] = pair<EthernetAddress, uint32_t> { msg_src_mac, 30000 };
      arp_table_waiting_.erase( msg_src_ip );

      if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST
           && arp_message.target_ip_address == ip_address_.ipv4_numeric() ) {
        send_ARPReply( msg_src_ip );
      }

      // Find the datagram with the destination IP address in the ARP table
      auto it = dgram_table_.find( msg_src_ip );
      if ( it != dgram_table_.end() ) {
        for ( auto dgram : it->second ) {
          send_InternetDatagram( dgram.first, msg_src_ip );
        }
        dgram_table_.erase( it );
      }
      break;
    }

    default:
      debug( "default datagram received" );
      break;
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    if ( it->second.second <= ms_since_last_tick ) {
      it = arp_table_.erase( it );
    } else {
      it->second.second -= ms_since_last_tick;
      ++it;
    }
  }

  for ( auto it = dgram_table_.begin(); it != dgram_table_.end(); ) {
    auto& vec = it->second;
    for ( auto iter = vec.begin(); iter != vec.end(); ) {
      if ( iter->second <= ms_since_last_tick ) {
        iter = vec.erase( iter ); // 返回的是下一个有效迭代器
      } else {
        iter->second -= ms_since_last_tick;
        ++iter;
      }
    }
    if ( it->second.empty() ) {
      it = dgram_table_.erase( it );
    } else {
      ++it;
    }
  }

  vector<uint32_t> to_resend;
  for ( auto it = arp_table_waiting_.begin(); it != arp_table_waiting_.end(); ) {
    if ( it->second <= ms_since_last_tick ) {
      to_resend.push_back( it->first );
      it = arp_table_waiting_.erase( it );
    } else {
      it->second -= ms_since_last_tick;
      ++it;
    }
  }

  for ( auto ip : to_resend ) {
    send_ARPRequest( ip );
  }
}
