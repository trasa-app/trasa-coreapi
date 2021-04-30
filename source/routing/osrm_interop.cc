// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <iostream>
#include <unordered_map>

#include <osrm/osrm.hpp>
#include <osrm/coordinate.hpp>
#include <osrm/json_container.hpp>
#include <osrm/trip_parameters.hpp>

#include "worker.h"
#include "waypoint.h"
#include "osrm_interop.h"

void debug_trip(osrm::json::Object& trip, size_t i)
{
  auto& legs = trip.values["legs"].get<osrm::json::Array>().values;
  std::clog << "trips[" << i << "].legs.count = " << legs.size() << std::endl;

  for (size_t j = 0; j < legs.size(); ++j) {
    auto leg = legs.at(j).get<osrm::json::Object>();
    auto& distance = leg.values["distance"].get<osrm::json::Number>().value;
    auto& duration = leg.values["duration"].get<osrm::json::Number>().value;
    
    std::clog << "trips[" << i << "].legs[" << j << "].distance = " << distance << std::endl;
    std::clog << "trips[" << i << "].legs[" << j << "].duration = " << duration << std::endl;
    std::clog << std::endl;
  }
}

void debug_waypoint(osrm::json::Object& waypoint, size_t i)
{
  auto& name = waypoint.values["name"].get<osrm::json::String>().value;
  std::clog << "waypoint[" << i << "].name = " << name << std::endl;

  auto& tripix = waypoint.values["trips_index"].get<osrm::json::Number>().value;
  std::clog << "waypoint[" << i << "].trips_index = " << tripix << std::endl;

  auto& waypointix = waypoint.values["waypoint_index"].get<osrm::json::Number>().value;
  std::clog << "waypoint[" << i << "].waypoint_index = " << waypointix << std::endl;
  
  std::clog << std::endl;
}

void debug_output(osrm::json::Object& json_result)
{
  auto& code = json_result.values["code"].get<osrm::json::String>().value;
  std::clog << "status code: " << code << std::endl;

  auto& waypoints = json_result.values["waypoints"].get<osrm::json::Array>().values;
  std::clog << "waypoints.count = " << waypoints.size() << std::endl;

  for (size_t i = 0; i < waypoints.size(); ++i) {
    auto& waypoint = waypoints.at(i).get<osrm::json::Object>();
    debug_waypoint(waypoint, i);
  }

  std::clog << std::endl;

  auto& trips = json_result.values["trips"].get<osrm::json::Array>().values;
  std::clog << "trips.count = " << trips.size() << std::endl;

  for (size_t i = 0; i < trips.size(); ++i) {
    auto& trip = trips.at(i).get<osrm::json::Object>();
    debug_trip(trip, i);
  }
}

namespace sentio::routing 
{

class osrm_instance::impl 
{
public:
  impl(config const &cfg, import::region_paths const &source)
    : engconfig_{
        .storage_config =
          osrm::storage::StorageConfig(source.osrm),
        .max_locations_trip = static_cast<int>(cfg.max_waypoints),
        .use_shared_memory = false,
        .memory_file = boost::filesystem::path(),
        .algorithm = cfg.algorithm,
        .verbosity = "DEBUG",
        .dataset_name = source.name}
    , engineinstance_(engconfig_) 
    {
      std::clog << "created routing engine instance for " 
                << source.name << " using index: "
                << source.osrm << std::endl;
    }

public:
  /**
   * Converts from osrm formatted list of legs
   * into our internal version of that list. 
   * 
   * It walks over the list of legs in the osrm output to extrac
   * leg travel costs, it alse leaves the (from/to)_building fields
   * zeroed, they will be assigned to the correct values in the 
   * optimized trip contructed, once waypoints are sorted by their order
   */
  static std::vector<route_leg> map_legs(osrm::json::Object& json_result)
  {
    auto& trips = json_result.values["trips"].get<osrm::json::Array>().values;
    if (trips.size() != 1) {
      throw std::runtime_error("multipart trips are not supported yet.");
    }

    auto& legs = trips.at(0).get<osrm::json::Object>()
      .values["legs"].get<osrm::json::Array>().values;

    std::vector<route_leg> output;
    for (size_t i = 0; i < legs.size(); ++i) {

      auto& leg = legs.at(i).get<osrm::json::Object>();
      auto& distance = leg.values["distance"].get<osrm::json::Number>().value;
      auto& duration = leg.values["duration"].get<osrm::json::Number>().value;
      
      // here (from/to)_building values are not assigned, they
      // will be assigned in the optimized_trip constructor once
      // the waypoints are in the correct optimal order
      output.push_back(route_leg {
        .from_building = 0,
        .to_building = 0,
        .cost = travel_cost {
          .distance = static_cast<int>(distance),
          .duration = std::chrono::seconds(static_cast<int>(duration))
        }
      });
    }

    return output;
  }

  static polyline map_geometry(osrm::json::Object& json_result)
  {
    auto& trips = json_result.values["trips"].get<osrm::json::Array>().values;
    assert(trips.size() == 1);

    auto& trip = trips.at(0).get<osrm::json::Object>();
    auto& polylinestring = trip.values["geometry"].get<osrm::json::String>().value;
    return polyline(std::move(polylinestring));
  }

  static optimized_trip::indecies_container map_waypoints_order(osrm::json::Object& json_result)
  {
    optimized_trip::indecies_container output;
    auto& waypoints = json_result.values["waypoints"].get<osrm::json::Array>().values;
    for (size_t i = 0; i < waypoints.size(); ++i) {
      auto wix = waypoints.at(i)
        .get<osrm::json::Object>().values["waypoint_index"]
        .get<osrm::json::Number>().value;
      output.push_back(wix);
    }
    return output;
  }

  optimized_trip optimize_trip(unoptimized_trip trip) const 
  {
    osrm::TripParameters tparams;
    tparams.roundtrip = trip.roundtrip();
    tparams.overview = osrm::RouteParameters::OverviewType::Full;
    tparams.source = osrm::engine::api::TripParameters::SourceType::First;

    if (trip.roundtrip()) {
      tparams.destination = osrm::engine::api::TripParameters::DestinationType::Any;
    } else {
      tparams.destination = osrm::engine::api::TripParameters::DestinationType::Last;
    }

    for (auto const& waypoint: trip) {
      tparams.coordinates.push_back({
        osrm::util::FloatLongitude{waypoint.building.coords.longitude()},
        osrm::util::FloatLatitude{waypoint.building.coords.latitude()},
      });
    }

    // for roundtrips, the last waypoint should not be a duplicate in osrm format
    if (trip.roundtrip() && 
        tparams.coordinates.back().lat == tparams.coordinates.front().lat &&
        tparams.coordinates.back().lon == tparams.coordinates.front().lon) {
          tparams.coordinates.resize(
            tparams.coordinates.size() - 1);
    }

    osrm::engine::api::ResultT result = osrm::json::Object();
    const auto status = engineinstance_.Trip(tparams, result);
    auto& json_result = result.get<osrm::json::Object>();
    auto const& returncode = json_result.values["code"].get<osrm::json::String>().value;
    if (status == osrm::Status::Ok && boost::iequals(returncode, "ok")) {
      auto waypointsorder = map_waypoints_order(json_result);
      std::cout << "trip_size_3: " << trip.size() << std::endl;
      return optimized_trip(
        trip, waypointsorder,
        map_legs(json_result),
        map_geometry(json_result));
    } else {
      const auto code = json_result.values["code"].get<osrm::json::String>().value;
      const auto message = json_result.values["message"].get<osrm::json::String>().value;
      std::cerr << "trip optimization failed. code: "  
                << code << ", message: " << message << std::endl;
      throw std::runtime_error(message);
    }
  }

  travel_cost calculate_distance(
    spacial::coordinates const& from,
    spacial::coordinates const& to) const
  {
    osrm::RouteParameters rparams;
    rparams.coordinates.push_back({
      osrm::util::FloatLongitude(from.longitude()),
      osrm::util::FloatLatitude(from.latitude())});
    rparams.coordinates.push_back({
      osrm::util::FloatLongitude(to.longitude()),
      osrm::util::FloatLatitude(to.latitude())});
    
    osrm::engine::api::ResultT result = osrm::json::Object();
    const auto status = engineinstance_.Route(rparams, result);
    if (status == osrm::Status::Ok) {
      auto& route = result.get<osrm::json::Object>()
        .values["routes"].get<osrm::json::Array>()
        .values.at(0).get<osrm::json::Object>();
      auto duration = static_cast<size_t>(
        route.values["duration"].get<osrm::json::Number>().value);
      return travel_cost {
        .distance = route.values["distance"].get<osrm::json::Number>().value,
        .duration = std::chrono::seconds(duration)
      };
    } else {
      throw std::runtime_error("routing failed");
    }
    
  }

private:
  osrm::EngineConfig engconfig_;
  osrm::OSRM engineinstance_;
};

osrm_instance::~osrm_instance() = default;
osrm_instance::osrm_instance(config const &cfg,
  import::region_paths const &source)
  : impl_(std::make_unique<impl>(cfg, source)) {}

optimized_trip osrm_instance::optimize_trip(unoptimized_trip trip) const 
{ return impl_->optimize_trip(std::move(trip)); }

travel_cost osrm_instance::calculate_distance(
    spacial::coordinates const& from,
    spacial::coordinates const& to) const
{ return impl_->calculate_distance(from, to); }

class osrm_map::impl {
public:
  impl(config const& config, std::vector<import::region_paths> const& sources)
  {
    for (auto const& source: sources) {
      try {
        instances_.emplace(source.name, 
          osrm_instance(config, source));
      } catch (std::exception const& e) {
        std::clog << "failed to create routing engine instance for "
                << source.name << " using index: " << source.osrm 
                << ". reason: " << e.what() << std::endl;
        throw;
      }
    }
  }

public:
  optimized_trip optimize_trip(
    unoptimized_trip trip, std::string const& region) const 
  {
    auto instanceit = instances_.find(region);
    if (instanceit == instances_.end()) {
      throw std::runtime_error("invalid region");
    }
    return instanceit->second.optimize_trip(std::move(trip));
  }

  travel_cost calculate_distance(
    spacial::coordinates const& from,
    spacial::coordinates const& to, 
    std::string const& region) const
  {
    auto instanceit = instances_.find(region);
    if (instanceit == instances_.end()) {
      throw std::runtime_error("invalid region");
    }
    return instanceit->second.calculate_distance(from, to);
  }

private:
  std::unordered_map<std::string, osrm_instance> instances_;
};

osrm_map::~osrm_map() = default;
osrm_map::osrm_map(config const &config,
  std::vector<import::region_paths> const& sources)
  : impl_(std::make_unique<impl>(config, sources)) {}

optimized_trip osrm_map::optimize_trip(
  unoptimized_trip trip, std::string const& region) const 
{ return impl_->optimize_trip(std::move(trip), region); }

travel_cost osrm_map::calculate_distance(
    spacial::coordinates const& from,
    spacial::coordinates const& to,
    std::string const& region) const
{ return impl_->calculate_distance(from, to, region); }

} // namespace sentio::routing

