#pragma once

#include "address.hh"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>

inline uint8_t get_bit( uint32_t ip, uint8_t bit_index )
{
  return ( ip >> ( 31 - bit_index ) ) & 1;
}

class PrefixMatchTree
{
private:
  struct Node
  {
    uint32_t prefix;
    uint8_t prefix_length;
    std::optional<Address> next_hop;
    size_t interface_num;

    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right;

    bool is_route = false;

    Node()
      : prefix( 0 )
      , prefix_length( 0 )
      , next_hop( std::nullopt )
      , interface_num( 0 )
      , left( nullptr )
      , right( nullptr )
    {}
  };

  std::shared_ptr<Node> root_;

public:
  PrefixMatchTree() : root_( std::make_shared<Node>() ) {}

  void insert_node( uint32_t prefix, uint8_t prefix_length, std::optional<Address> next_hop, size_t interface_num );

  std::pair<std::optional<Address>, size_t> longest_prefix_match( uint32_t ip );
};