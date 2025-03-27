#include "prefix_match_tree.hh"

using namespace std;

void PrefixMatchTree::insert_node( uint32_t prefix,
                                   uint8_t prefix_length,
                                   std::optional<Address> next_hop,
                                   size_t interface_num )
{
  auto node = root_;
  for ( int i = 0; i < prefix_length; ++i ) {
    uint8_t bit = get_bit( prefix, i );
    if ( bit == 0 ) {
      if ( !node->left )
        node->left = make_shared<Node>();
      node = node->left;
    } else {
      if ( !node->right )
        node->right = make_shared<Node>();
      node = node->right;
    }
  }
  node->prefix = prefix;
  node->prefix_length = prefix_length;
  node->next_hop = next_hop;
  node->interface_num = interface_num;
  node->is_route = true;
}

pair<optional<Address>, size_t> PrefixMatchTree::longest_prefix_match( uint32_t ip )
{
  auto node = root_;
  pair<optional<Address>, size_t> best_match {};
  // Add "&& node" to the judgement condition.
  for ( int i = 0; i < 32 && node; ++i ) {
    if ( node->is_route )
      best_match = make_pair( node->next_hop, node->interface_num );

    uint8_t bit = get_bit( ip, i );
    node = bit == 0 ? node->left : node->right;
  }
  return best_match;
}