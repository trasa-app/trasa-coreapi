// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <chrono>
#include <optional>

#include "model/address.h"

namespace sentio::routing
{

/**
 * Represents a single address that needs to be visited during a trip.
 * 
 * It adds trip-specific information on top of an absolute building object,
 * such as a phone number associated with the contact person in that building
 * the input method used to enter the address, etc.
 */
struct waypoint 
{
public:
  waypoint(
    model::building bldg, 
    std::optional<std::string> phone = {},
    std::optional<std::string> imethod = {},
    std::optional<std::string> notes = {});
  
  waypoint(json_t const&);

public:
  model::building building;
  std::optional<std::string> phone;
  std::optional<std::string> input_method;
  std::optional<std::string> notes;

public:
  json_t to_json() const;
};

/**
 * Represents the time and distance needed to travel between two waypoints.
 */
struct travel_cost
{
  int distance;
  std::chrono::seconds duration;
};

/**
 * Represents a single leg of the route between two waypoints.
 */
struct route_leg
{
  int64_t from_building;
  int64_t to_building;
  travel_cost cost;

  json_t to_json() const;
};

/**
 * Represents a polyline that describes the driving directions
 * of a trip. This is used when rendering the map for the end-used.
 */
class polyline
{
public:
  polyline(std::string serialized);
  std::string const& serialized() const;
  operator std::string() const;
private:
  std::string serialized_;
};

}  // namespace sentio::model