// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/register/point.hpp>

#include "street.h"
#include "trie/trie_policy.hpp"
#include "trie/tag_and_trait.hpp"
#include "trie/assoc_container.hpp"

namespace sentio::geocoder
{
namespace pbds = __gnu_pbds;

/**
 * Locates streets and cities within a region.
 *
 * This is the smalles unit of map splitting. It usually encapsulates
 * a single county, voyodship or otherwise geographic location that
 * has cities, towns and streets.
 */
class region final
{
public:  // domain types
  using key_type = std::string;
  using mapped_type = std::map<std::string, street>;

public:  // trie configuration
  using tag = pbds::pat_trie_tag;
  using comp = pbds::trie_string_access_traits<key_type>;
  using trie = pbds::trie<key_type, mapped_type, comp, tag,
                          pbds::trie_prefix_search_node_update>;

public:  // public iteration types
  using iterator = trie::const_iterator;
  using range = std::pair<iterator, iterator>;

public:
  region(std::string name);
  ~region();

public: // move only semantics
  region(region&&);
  region& operator=(region&&);

private: // disable copying
  region(region const&) = delete;
  region& operator=(region const&) = delete;

public:  // iterators over streets in a region
  size_t size() const;
  iterator begin() const;
  iterator end() const;

public:  // region properties
  std::string name() const;

public:  // searches for streets in a region
  iterator find(std::string const& exact) const;
  range prefix_match(std::string const& prefix) const;

public:
  /**
   * Adds a building to the region index.
   *
   * This index will first store the street name as a prefix
   * path in a trie for fast prefix-string lookup by street name.
   *
   * Then in value nodes it will store a map that maps city names
   * where this street name occurs, then it will forward the building
   * object to the correct <street,city> entry and insert it into a
   * street.
   */
  void insert(model::building b);
  
  /**
   * Optionally called after its known that no further inserts
   * are going to happen for this region. This gives the underlying
   * index a chance to optimzie itself for efficient read-only access
   * moving forward. 
   * 
   * Calling "insert" after this method is called yields undefined 
   * behaviour.
   */
  void seal();

private:
  class impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace sentio::kurier