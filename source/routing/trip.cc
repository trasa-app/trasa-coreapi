// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <sstream>
#include <numeric>
#include <iostream>

#include <boost/range/join.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/time_parsers.hpp>

#include "trip.h"
#include "utils/meta.h"

#define prod_assert(x) if (!(x)) {     \
  throw std::runtime_error(            \
    "production assertion faile: "#x); \
  }

namespace sentio::routing
{

static std::vector<waypoint> 
waypoints_from_json_list(json_t const& wps)
{
  std::vector<waypoint> output;
  for (auto const& wp : wps) {
    try {
      output.emplace_back(wp.second);
    } catch (...) {
      std::stringstream ss;
      boost::property_tree::write_json(ss, wp.second);
      std::cerr << "parsing waypoint failed at: " << ss.str() << std::endl;
      throw;
    }
  }
  return output;
}

/**
 * Locates a waypoint with a given building id, 
 * removes it from the json object and returns
 * the waypoint object. This is used to extract
 * start/end points from the trip so they could 
 * be treated differently.
 */
std::optional<waypoint> extract_waypoint(
  json_t& requestbody,
  std::string const& buildingid) 
{
  auto wpit = requestbody.get_child("waypoints").begin();
  auto wpend = requestbody.get_child("waypoints").end();
  for (; wpit != wpend; ++wpit) {
    auto bid = wpit->second.get<std::string>("building.id");
    if (boost::iequals(bid, buildingid)) {
      waypoint target_waypoint(wpit->second);
      requestbody.get_child("waypoints").erase(wpit);
      return target_waypoint;
    }
  }
  return { };
}

auto find_waypoint(std::vector<waypoint>& wps, std::string const& id) 
{
  return std::find_if(wps.begin(), wps.end(), 
    [&id](auto const& waypoint) { 
      return std::to_string(waypoint.building.id) == id;
    });
}


//
// unoptimized_trip
//

unoptimized_trip::unoptimized_trip(
  waypoint starting_point,
  waypoint final_point,
  waypoints_container waypoints)
{ // merge all points into one collection
  waypoints_.push_back(std::move(starting_point));
  waypoints_.insert(waypoints_.end(), 
    std::make_move_iterator(waypoints.begin()),
    std::make_move_iterator(waypoints.end()));
  waypoints_.push_back(std::move(final_point));
}

unoptimized_trip::unoptimized_trip(json_t body)
  : unoptimized_trip(
      waypoint(body.get_child("starting_point")),
      waypoint(body.get_child("final_point")),
      waypoints_from_json_list(body.get_child("waypoints")))
{}

bool unoptimized_trip::roundtrip() const
{ return starting_waypoint().building.id == final_waypoint().building.id; }

waypoint const& unoptimized_trip::starting_waypoint() const
{ return waypoints_.front(); }

waypoint const& unoptimized_trip::final_waypoint() const
{ return waypoints_.back(); }

size_t unoptimized_trip::size() const 
{ return waypoints_.size(); }

waypoint const& unoptimized_trip::operator[](size_t index) const
{ return waypoints_[index]; }

unoptimized_trip::iterator unoptimized_trip::begin() const
{ return waypoints_.begin(); }

unoptimized_trip::iterator unoptimized_trip::end() const
{ return waypoints_.end(); }

json_t unoptimized_trip::to_json() const
{
  json_t output;
  output.add_child("starting_point", waypoints_.front().to_json());
  output.add_child("final_point", waypoints_.back().to_json());
  
  json_t waypointsvec;
  auto waypointsit = waypoints_.begin() + 1;
  auto waypointsend = waypoints_.end() - 1;
  for (; waypointsit != waypointsend; ++waypointsit) {
    waypointsvec.push_back(std::make_pair("", waypointsit->to_json()));
  }

  output.add_child("waypoints", std::move(waypointsvec));
  return output;
}

//
// optimized_trip
//

optimized_trip::optimized_trip(
      unoptimized_trip original,
      indecies_container order,
      legs_container legs,
      polyline geometry)
  : unoptimized_trip(std::move(original))
  , geometry_(std::move(geometry)) 
  , legs_(std::move(legs))
{
  size_t expectedixs = roundtrip() 
    ? order.size() + 1 : order.size();

  if (expectedixs != size()) {
    throw std::invalid_argument(
      "indecies collection must be the "
      "same length as waypoints");
  }

  if (legs_.size() != (size() - 1)) {
    throw std::runtime_error(
      "trip leg count is not valid");
  }

  // Reorder the waypoiots collection according
  // to the order collection. In an optimized trip
  // waypoints should be always in the correct 
  // optimal order to match the legs collection. O(n)
  for (size_t i = 0; i < order.size(); ++i) {
    while (i != order[i]) {
      int alt = order[i];
      std::swap(waypoints_[i], waypoints_[alt]);
      std::swap(order[i], order[alt]);
    }
  }

  // now that we have waypoints in the correct
  // order, assign the (to/from)_building values
  // to legs
  for (size_t i = 0; i < legs_.size(); ++i) {
    size_t to_ix = i + 1;
    
    if (roundtrip() && i == (size() - 1)) {
      // for roundtrip, last to_building in 
      // the leg goes back to starting point
      to_ix = 0;
    }

    legs_[i].from_building = waypoints_[i].building.id;
    legs_[i].to_building = waypoints_[to_ix].building.id;
  }
}

polyline const& optimized_trip::geometry() const 
{ return geometry_; }

std::vector<route_leg> const& optimized_trip::legs() const 
{ return legs_; }

travel_cost optimized_trip::total_cost() const
{
  return std::accumulate(
    legs().begin(), legs().end(), travel_cost{},
    [](auto const& left, auto const& right) {
      return travel_cost { 
        .distance = left.distance + right.cost.distance, 
        .duration = left.duration + right.cost.duration
      };
    });
}

json_t optimized_trip::to_json() const
{
  json_t output = unoptimized_trip::to_json();
  json_t legsvec;
  for (auto const& leg: legs()) {
    legsvec.push_back(std::make_pair("", leg.to_json()));
  }
  output.add_child("legs", std::move(legsvec));
  output.add("geometry", geometry_.serialized());
  return output;
}

//
// trip_metadata
//

trip_metadata::trip_metadata()
 : createdat_(boost::posix_time::second_clock::universal_time()) {}

trip_metadata::trip_metadata(json_t const& json)
  : region_(json.get<std::string>("region"))
  , accountid_(json.get<std::string>("accountid"))
  , createdat_(boost::posix_time::from_iso_string(json.get<std::string>("createdat")))
  , id_(to_std(json.get_optional<std::string>("id")))
  , receipthandle_(to_std(json.get_optional<std::string>("receipthandle")))
{
}

std::optional<std::string> const& trip_metadata::id() const
{ return id_; }

std::optional<std::string> const& trip_metadata::receipthandle() const
{ return receipthandle_; }

std::string const& trip_metadata::accountid() const 
{ return accountid_; }

date_time_t trip_metadata::created_at() const 
{ return createdat_; }

std::string const& trip_metadata::region() const 
{ return region_; }

json_t trip_metadata::to_json() const
{
  json_t output;
  output.add("id", id().value_or(""));
  output.add("region", region());
  output.add("accountid", accountid());
  output.add("receipthandle", receipthandle().value_or(""));
  output.add("createdat", boost::posix_time::to_iso_string(created_at()));
  return output;
}

//
// trip_request
//

trip_request::trip_request(json_t b)
  : body_(std::move(b))
  , trip_(body_)
  , metadata_(body_.get_child("meta"))
  , location_(spacial::coordinates::from_json(body_.get_child("location")))
{
  verify_argument(!meta().region().empty());
  verify_argument(!meta().accountid().empty());

  verify_argument(trip().size() >= 3);
  verify_argument(trip().final_waypoint().building.id != 0);
  verify_argument(trip().starting_waypoint().building.id != 0);
}

trip_metadata const& trip_request::meta() const
{ return metadata_; }

spacial::coordinates const& trip_request::location() const 
{ return location_; }

unoptimized_trip const& trip_request::trip() const
{ return trip_; }

json_t const& trip_request::to_json() const
{ return body_; }

//
// trip_response
//

trip_response::trip_response(optimized_trip trip, trip_metadata metadata)
  : meta_(std::move(metadata))
  , trip_(std::move(trip))
{
  auto expectedlegcount = trip_.roundtrip()
    ? trip_.size() : trip_.size() - 1;

  prod_assert(trip_.size() >= 3);
  prod_assert(trip_.legs().size() == expectedlegcount);
  prod_assert(!trip_.geometry().serialized().empty());

  // waypoints are < 300, so this is fast enough
  // for (auto const& leg: trip_.legs()) {
  //   if (leg.from_building == request.starting_waypoint().building.id) {
  //     waypoints_.emplace_back(request.starting_waypoint());
  //   } else if (leg.from_building == request.final_waypoint().building.id) {
  //     waypoints_.emplace_back(request.final_waypoint());
  //   } else {
  //     auto wit = std::find_if(
  //       request_.waypoints().begin(), request_.waypoints().end(), 
  //       [&leg](auto const& wp) { return leg.from_building == wp.building.id; });
  //     assert(wit != request_.waypoints().end());
  //     waypoints_.emplace_back(*wit);
  //   }
  // }

  // last waypoint
  // auto wit = std::find_if(
  //   request_.waypoints().begin(), request_.waypoints().end(), 
  //   [this](auto const& wp) { return legs_.back().to_building == wp.building.id; });
  // assert(wit != request_.waypoints().end());
  // waypoints_.emplace_back(*wit);

  // auto expectedwaypointcount = request_.roundtrip()
  //   ? request_.waypoints().size() + 1
  //   : request_.waypoints().size();

  // prod_assert(waypoints_.size() == expectedwaypointcount);
}

trip_metadata const& trip_response::meta() const
{ return meta_; }

optimized_trip const& trip_response::trip() const
{ return trip_; }

json_t trip_response::to_json() const
{ 
  json_t output;
  output.add_child("meta", meta().to_json());
  output.add_child("trip", trip().to_json());
  return output;
}

}
