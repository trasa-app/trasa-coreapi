// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <boost/geometry/algorithms/within.hpp>

#include "region.h"

namespace sentio::spacial
{

region::region(std::string name, region::polygon bounds)
  : name_(std::move(name)), bounds_(std::move(bounds)) 
{ }

std::string const& region::name() const
{ return name_; }

region::polygon const& region::bounds() const 
{ return bounds_; }

bool region::contains(region::point const& p) const 
{ return boost::geometry::within(p, bounds_); }

bool region::operator==(region const& other) const
{ return name_ == other.name_; }

bool region::operator!=(region const& other) const 
{ return !(*this == other); }

}
