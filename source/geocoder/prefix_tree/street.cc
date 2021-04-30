// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <cassert>
#include <iostream>

#include "street.h"

namespace sentio::geocoder
{
template <typename T>
void inline throw_if_not_equal(T left, T right, const char* field)
{
  if (left != right) {
    throw std::invalid_argument(field);
  }
}

void optionally_report_duplicate(street const& s, model::building const& b) 
{
  if (s.find(b.number) != s.end()) {
    std::cerr << "duplicate house number " << b.number 
              << " on street " << b.street << " in "
              << b.city << std::endl;
  }
}

//street::street(std::string name, std::string city)
street::street(std::string name)
    : name_(std::move(name))
    //, city_(std::move(city))
{
  assert(!name_.empty());
  //assert(!city_.empty());
}

std::string const& street::name() const { return name_; }
//std::string const& street::city() const { return city_; }

street::iterator street::begin() const
{
  return buildings_.begin();
}

street::iterator street::end() const { return buildings_.end(); }

size_t street::size() const { return buildings_.size(); }

street::iterator street::find(std::string const& exact) const
{
  return buildings_.find(exact);
}

street::range street::prefix_match(std::string const& prefix) const
{
  return buildings_.prefix_range(prefix);
}

void street::insert(model::building b)
{
  throw_if_not_equal(b.street, name(), "street");

  if (find(b.number) != end()) { //optionally_report_duplicate(*this, b);
    // see https://github.com/sentio-systems/kurier/issues/1
    // skip inserting duplicate coordinates for the same building.
    // figure out a way to aggregate them into a polygon and use 
    // their centroid as the coordinate. This issue happens with 
    // large buildings with multiple entrances that span large ares.
    return; 
  }

  throw_if_not_equal(b.number.empty(), false, "number");
  throw_if_not_equal(b.coords.empty(), false, "coords");

  std::string number = b.number;
  buildings_.insert(std::make_pair(number, building_leaf {
    .id = b.id,
    .zipcode = b.zipcode,
    .coords = b.coords
  }));
}

}  // namespace sentio::kurier