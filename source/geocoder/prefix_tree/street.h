// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <map>
#include <utility>

#include "model/address.h"
#include "trie/trie_policy.hpp"
#include "trie/tag_and_trait.hpp"
#include "trie/assoc_container.hpp"

namespace sentio::geocoder
{
namespace pbds = __gnu_pbds;

struct building_leaf {
  int64_t id;
  std::string zipcode;
  spacial::coordinates coords;
};

/**
 * Locates buildings and their coordinates within on street.
 *
 * This index is specific to a unique street within a known city.
 * This index allows users to do a prefix search on building numbers,
 * once a valid street <--> city pair is found.
 */
class street
{
public:
  using key_type = std::string;
  using value_type = building_leaf;
  using mapped_type = building_leaf;

private:
  using tag = pbds::pat_trie_tag;
  using comp = pbds::trie_string_access_traits<key_type>;
  using container = pbds::trie<key_type, mapped_type, comp, tag,
                               pbds::trie_prefix_search_node_update>;

public:
  using iterator = container::const_iterator;
  using range = std::pair<iterator, iterator>;

public:
  street(std::string name);

public:  // this street info
  std::string const& name() const;

public:  // iterates over buildings in a street
  iterator begin() const;
  iterator end() const;
  size_t size() const;

public:  // searches for buildings in a street
  iterator find(std::string const& exact) const;
  range prefix_match(std::string const& prefix) const;

public:
  /**
   * Adds a building to the current street.
   *
   * This method will store buildings belonging to this
   * street, addressed by their number.
   */
  void insert(model::building b);

private:
  std::string name_;
  container buildings_;
};

}  // namespace sentio::kurier
