// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include "coords.h"

namespace sentio::spacial
{
/**
 * Represents a separate are in the spacial index. 
 * This is used to separate the map into distinct submaps
 * for performance optimization purposes and geofencing.
 */
class region
{
public:
  using point = spacial::coordinates;
  using polygon = boost::geometry::model::polygon<point>;

public:
  polygon const& bounds() const;
  std::string const& name() const;
  bool contains(point const& p) const;

public:
  region(std::string name, polygon bounds);

public:
  bool operator==(region const& other) const;
  bool operator!=(region const& other) const;

private:
  std::string name_;
  polygon bounds_;
};

}