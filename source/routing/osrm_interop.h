// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <memory>

#include "trip.h"
#include "import/map_source.h"

namespace sentio::routing
{

class config;

/**
 * This class represents an instance of the OSRM routing engine for 
 * one region. Instances of this class are created for the worker role
 * and process trip_reuests off the SQS queue of pending requests.
 * 
 * For an instance to work, it needs the name of the region it is
 * serving and a filepath to the osrm basefile that has preprocessed
 * map data.
 */
class osrm_instance
{
public:
  osrm_instance(config const& cfg,
    import::region_paths const& source);
  ~osrm_instance();

public:
  optimized_trip optimize_trip(unoptimized_trip request) const;
  travel_cost calculate_distance(
    spacial::coordinates const& from,
    spacial::coordinates const& to) const;

public:
  osrm_instance(osrm_instance&&) = default;
  osrm_instance& operator=(osrm_instance&&) = default;

private:
  class impl;
  std::unique_ptr<impl> impl_;
};

/**
 * Wraps a map of region_name <--> osrm instance and routes
 * trip requests to the appropriate instance of the OSRM 
 * engine based on the region. Right now we are just trusting
 * the region name from the metadata field in the trip request
 * in SQS, however other slicing algorithms might be expected.
 */
class osrm_map 
{
public:
  osrm_map(config const& config,
    std::vector<import::region_paths> const& sources);
  ~osrm_map();

public:
  optimized_trip optimize_trip(
    unoptimized_trip request, 
    std::string const& region) const;

  travel_cost calculate_distance(
    spacial::coordinates const& from,
    spacial::coordinates const& to,
    std::string const& region) const;

private:
  class impl;
  std::shared_ptr<impl> impl_;
};

}