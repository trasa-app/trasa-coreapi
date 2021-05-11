// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <vector>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include "utils/json.h"

namespace sentio::spacial
{
  
/**
 * Represents a single GPS coordinate (lat, lng).
 */
class coordinates
{
public:
  coordinates();
  coordinates(json_t const&);
  coordinates(double lat, double lng);

public:  // r/o
  double longitude() const;
  double latitude() const;

public:  // r/w
  double& longitude();
  double& latitude();

public:
  bool empty() const;

public:
  bool operator==(coordinates const& other) const;
  bool operator!=(coordinates const& other) const;

public:
  json_t to_json() const;
  static coordinates from_json(json_t const&);

private:
  double lat_, lng_;
};

}

/**
 * This registers the custom class `coordinates` type
 * as a point type with boost::geometry for use in all
 * geometric algorithms and rtree indecies. This avoids
 * having to convert them to point2d and unessessary copies.
 */
BOOST_GEOMETRY_REGISTER_POINT_2D(
  sentio::spacial::coordinates, double,
  boost::geometry::cs::geographic<boost::geometry::degree>, 
  longitude(), latitude())
