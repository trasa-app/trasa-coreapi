// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <thread>

#include "config.h"

namespace sentio::routing
{

config::config()
  : max_waypoints(500)
  , async_threshold(15)
  , worker_concurrency(std::thread::hardware_concurrency())
  , algorithm(osrm::EngineConfig::Algorithm::CH)
{
}

config::config(json_t const& json)
  : max_waypoints(json.get<uint64_t>("max_waypoints"))
  , async_threshold(json.get<uint64_t>("async_threshold"))
  , worker_concurrency(json.get<uint64_t>("worker_concurrency"))
{
  auto algostring = json.get<std::string>("algorithm");
  if (boost::iequals(algostring, "contraction hierarchies") ||
      boost::iequals(algostring, "contraction_hierarchies") ||
      boost::iequals(algostring, "ch")) {
    algorithm = osrm::EngineConfig::Algorithm::CH;
  } else if (boost::iequals(algostring, "multi-Level dijkstra") ||
             boost::iequals(algostring, "multiLevel_dijkstra") ||
             boost::iequals(algostring, "multiLevel dijkstra") ||
             boost::iequals(algostring, "mld")) {
    algorithm = osrm::EngineConfig::Algorithm::MLD;
  } else {
    throw std::invalid_argument("unrecognized routing algorithm");
  }
}

}
