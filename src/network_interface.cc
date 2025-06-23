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
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t next_hop_ip = next_hop.ipv4_numeric();

  // Check if we know the Ethernet address for this IP
  auto arp_entry = arp_table_.find( next_hop_ip );

  if ( arp_entry != arp_table_.end() ) { // if ethernet address is mapped
    EthernetAddress next_hop_ethernet = arp_entry->second.ethernet_addr;
    EthernetFrame frame {};
    frame.header.dst = next_hop_ethernet;
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_IPv4;

    // Serialize the datagram into the payload
    Serializer serializer;
    dgram.serialize( serializer );
    auto serialized_dgram = serializer.finish();
    frame.payload = std::move( serialized_dgram );

    transmit( frame );
  } else { // broadcast ARP request
    // Queue the datagram for later transmission
    pending_datagrams_[next_hop_ip].push( dgram );

    // Check if we've recently sent an ARP request for this IP
    if ( pending_arp_requests_.find( next_hop_ip )
         != pending_arp_requests_.end() ) { // there is a pending ARP request
      cout << "DEBUG: Recently sent ARP request for IP " << next_hop_ip << ", not sending another request yet\n";
      return;
    }

    ARPMessage arp_msg;
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg.sender_ethernet_address = ethernet_address_;
    arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
    arp_msg.target_ethernet_address = {}; // Unknown, set to all zeros
    arp_msg.target_ip_address = next_hop_ip;

    // Create Ethernet frame for ARP request
    EthernetFrame arp_frame;
    arp_frame.header.src = ethernet_address_;
    arp_frame.header.dst = ETHERNET_BROADCAST; // Broadcast
    arp_frame.header.type = EthernetHeader::TYPE_ARP;

    // Serialize ARP message
    Serializer arp_msg_serializer;
    arp_msg.serialize( arp_msg_serializer );
    auto serialized_arp_msg = arp_msg_serializer.finish();
    arp_frame.payload = std::move( serialized_arp_msg );
    transmit( arp_frame );

    // Record when we sent this ARP request
    pending_arp_requests_[next_hop_ip] = 0;
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  // ignore any frame not destined for the network interface
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    cerr << "DEBUG: Ignoring frame not destined for this interface: " << frame.header.to_string() << "\n";
    cerr << "network interface info" << ethernet_address_.data() << endl;
    return;
  }

  // Handle ARP messages
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    Parser parser( frame.payload );
    ARPMessage arp_msg;
    arp_msg.parse( parser );

    // Remember sender's Ethernet address and IP address for 30 seconds
    arp_table_[arp_msg.sender_ip_address] = { arp_msg.sender_ethernet_address, 0 };

    // Handle ARP REQUEST - send reply if it's asking for our IP
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
      // Send an ARP reply
      ARPMessage arp_reply;
      arp_reply.opcode = ARPMessage::OPCODE_REPLY;
      arp_reply.sender_ethernet_address = ethernet_address_;
      arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
      arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
      arp_reply.target_ip_address = arp_msg.sender_ip_address;

      // Create Ethernet frame for ARP reply
      EthernetFrame arp_frame;
      arp_frame.header.src = ethernet_address_;
      arp_frame.header.dst = arp_msg.sender_ethernet_address; // Send directly to requester
      arp_frame.header.type = EthernetHeader::TYPE_ARP;

      // Serialize ARP message
      Serializer arp_msg_serializer;
      arp_reply.serialize( arp_msg_serializer );
      auto serialized_arp_msg = arp_msg_serializer.finish();
      arp_frame.payload = std::move( serialized_arp_msg );
      transmit( arp_frame );
    }

    // Handle ARP REPLY - transmit any queued datagrams for this IP
    if ( arp_msg.opcode == ARPMessage::OPCODE_REPLY ) {
      // Check if we have any pending datagrams for this IP
      auto pending_it = pending_datagrams_.find( arp_msg.sender_ip_address );
      if ( pending_it != pending_datagrams_.end() ) {
        // Send all queued datagrams for this IP
        while ( !pending_it->second.empty() ) {
          const InternetDatagram& dgram = pending_it->second.front();

          // Create Ethernet frame for the datagram
          EthernetFrame reply_frame;
          reply_frame.header.dst = arp_msg.sender_ethernet_address;
          reply_frame.header.src = ethernet_address_;
          reply_frame.header.type = EthernetHeader::TYPE_IPv4;

          // Serialize the datagram into the payload
          Serializer serializer;
          dgram.serialize( serializer );
          auto serialized_dgram = serializer.finish();
          reply_frame.payload = std::move( serialized_dgram );

          transmit( reply_frame );
          pending_it->second.pop();
        }

        // Remove the empty queue
        pending_datagrams_.erase( pending_it );
      }

      // Remove from pending ARP requests
      pending_arp_requests_.erase( arp_msg.sender_ip_address );
    }
  }
  // Handle IPv4 datagrams
  else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    Parser ipv4_parser( frame.payload );
    dgram.parse( ipv4_parser );
    datagrams_received_.push( dgram );
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Clean up expired ARP table entries (30 seconds)
  auto arp_it = arp_table_.begin();
  while ( arp_it != arp_table_.end() ) {
    arp_it->second.time_elapsed += ms_since_last_tick;
    if ( arp_it->second.time_elapsed >= 30000 ) { // Remove expired entry
      uint32_t expired_ip = arp_it->first;
      arp_it = arp_table_.erase( arp_it );
      pending_datagrams_.erase( expired_ip );
    } else {
      ++arp_it;
    }
  }
  // Clean up expired pending ARP requests (5 seconds)
  auto pending_arp_it = pending_arp_requests_.begin();
  while ( pending_arp_it != pending_arp_requests_.end() ) {
    pending_arp_it->second += ms_since_last_tick;
    ;

    if ( pending_arp_it->second >= 5000 ) { // Remove expired ARP request
      uint32_t expired_ip = pending_arp_it->first;
      pending_arp_it = pending_arp_requests_.erase( pending_arp_it );
      pending_datagrams_.erase( expired_ip );
    } else {
      ++pending_arp_it;
    }
  }
}
