// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "waypoint.h"
#include "utils/meta.h"

#include <boost/property_tree/json_parser.hpp>

namespace sentio::routing
{

waypoint::waypoint(
  model::building bldg, 
  std::optional<std::string> telephone, 
  std::optional<std::string> imethod,
  std::optional<std::string> notes)
  : building(std::move(bldg))
  , phone(std::move(telephone))
  , input_method(std::move(imethod))
  , notes(std::move(notes)) 
{}

waypoint::waypoint(json_t const& wp)
  : waypoint(
      model::building::from_json(wp.get_child("building")),
      to_std(wp.get_optional<std::string>("phone")),
      to_std(wp.get_optional<std::string>("input_method")),
      to_std(wp.get_optional<std::string>("notes")))
{}

polyline::polyline(std::string serialized)
  : serialized_(std::move(serialized)) {}

std::string const& polyline::serialized() const
{ return serialized_; }

polyline::operator std::string() const
{ return serialized(); }

json_t waypoint::to_json() const 
{
  json_t output;
  output.add_child("building", building.to_json());
  
  if (phone.has_value() && !phone.value().empty()) {
    output.add("phone", phone.value());
  }

  if (notes.has_value() && !notes.value().empty()) {
    output.add("notes", notes.value());
  }

  if (input_method.has_value()) {
    output.add("input_method", input_method.value());
  }
  return output;
}

json_t route_leg::to_json() const 
{
  json_t output;
  output.add("from_building", from_building);
  output.add("to_building", to_building);
  output.add("cost.distance", cost.distance);
  output.add("cost.duration", cost.duration.count());
  return output;
}

}