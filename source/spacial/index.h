// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <list>
#include <utility>
#include <optional>

#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/register/point.hpp>

#include "region.h"
#include "import/map_source.h"

namespace sentio::spacial
{
/**
 * Locates a region in the world given a set of user coordinates.
 *
 * Each end-user geocoding request carries a set of user coordinates
 * that specify user's current location. Based on that value, the search
 * space is narrowed down the a smaller region that contains entries specific
 * to current user's region.
 */
class index
{
public:  // spacial index types & config
  using point = region::point;
  using bounds_type = boost::geometry::model::box<point>;
  using node_type = std::pair<bounds_type, region>;
  using rtree_algorithm = boost::geometry::index::rstar<16>;
  using rtree = boost::geometry::index::rtree<node_type, rtree_algorithm>;

public:
  /**
   * Give a set of coordinates, this method returns the region name 
   * index that contains the given point.
   *
   * This is needed to locate the target region based on user's location.
   * User's location is always part of the JSON-RPC request to the suggest
   * endpoint.
   *
   * If no region matches the given location, an empty optional is returned.
   */
  std::optional<region> locate(spacial::coordinates const&) const;


  void insert(region r);

public:
  index(std::vector<import::region_paths> const&);

// exposed only for unit tests
#ifdef UNIT_TEST_BUILD
public:
  index() {}
#endif

private:
  rtree regions_;
};

}  // namespace sentio::spacial
