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

  // Validate interface number
  if (interface_num >= interfaces_.size()) {
    throw std::runtime_error("Invalid interface number: " + std::to_string(interface_num));
  }
  
  // Create and add the routing entry
  RouteEntry entry;
  entry.route_prefix = route_prefix;
  entry.prefix_length = prefix_length;
  entry.next_hop = next_hop;
  entry.interface_num = interface_num;
  
  routing_table_.push_back(entry);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Process datagrams from all interfaces
  for (auto& interface : interfaces_) {
    auto& datagrams_queue = interface->datagrams_received();
    
    // Process all datagrams in the queue
    while (!datagrams_queue.empty()) {
      InternetDatagram dgram = std::move(datagrams_queue.front());
      datagrams_queue.pop();
      
      // Check and decrement TTL
      if (dgram.header.ttl <= 1) {
        // TTL is 0 or will become 0 after decrement - drop the datagram
        cerr << "DEBUG: Dropping datagram due to TTL expiry" << endl;
        continue;
      }
      
      // Decrement TTL
      dgram.header.ttl--;
      
      // Find the longest-prefix match route
      const RouteEntry* best_route = nullptr;
      uint8_t longest_prefix = 0;
      uint32_t dest_ip = dgram.header.dst;
      
      for (const auto& route : routing_table_) {
        // Check if this route matches the destination
        if (matches_route(dest_ip, route.route_prefix, route.prefix_length)) {
          // Check if this is a longer prefix match
          if (route.prefix_length >= longest_prefix) {
            longest_prefix = route.prefix_length;
            best_route = &route;
          }
        }
      }
      
      // If no route found, drop the datagram
      if (best_route == nullptr) {
        cerr << "DEBUG: No route found for destination " 
             << Address::from_ipv4_numeric(dest_ip).ip() << ", dropping datagram" << endl;
        continue;
      }
      
      // Determine the next hop address
      optional<Address> next_hop_addr;
      if (best_route->next_hop.has_value()) {
        // Use the specified next hop (gateway)
        next_hop_addr = best_route->next_hop;
      } else {
        // Direct delivery - use the datagram's destination as next hop
        next_hop_addr = Address::from_ipv4_numeric(dest_ip);
      }
      
      // Send the datagram through the appropriate interface
      try {
        interfaces_[best_route->interface_num]->send_datagram(dgram, next_hop_addr.value());
        cerr << "DEBUG: Routed datagram to " << next_hop_addr.value().ip()
             << " via interface " << best_route->interface_num << endl;
      } catch (const exception& e) {
        cerr << "DEBUG: Failed to send datagram: " << e.what() << endl;
      }
    }
  }
}

// Helper function to check if a destination IP matches a route prefix
bool Router::matches_route(uint32_t dest_ip, uint32_t route_prefix, uint8_t prefix_length)
{
  // Handle the special case of prefix_length = 0 (default route)
  if (prefix_length == 0) {
    return true;
  }
  
  // Handle the case where prefix_length = 32 (exact match)
  if (prefix_length == 32) {
    return dest_ip == route_prefix;
  }
  
  // Create a mask with the first prefix_length bits set to 1
  // Avoid undefined behavior when shifting by 32
  uint32_t mask = ~((1ULL << (32 - prefix_length)) - 1);
  
  // Apply the mask and compare
  return (dest_ip & mask) == (route_prefix & mask);
}
