#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <optional>

// Routing table entry
struct RouteEntry {
  uint32_t route_prefix = 0;      // Network address
  uint8_t prefix_length = 0;      // Subnet mask length
  std::optional<Address> next_hop {}; // Next hop address (empty for direct)
  size_t interface_num = 0;       // Interface index
};

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:

  
  // Routing table
  std::vector<RouteEntry> routing_table_ {};
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};
};
