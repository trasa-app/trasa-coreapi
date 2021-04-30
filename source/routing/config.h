// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include "utils/json.h"

#include <osrm/engine_config.hpp>
#include <boost/property_tree/ptree.hpp>

namespace sentio::routing
{

class config {
public:
  uint64_t max_waypoints;
  uint64_t async_threshold;
  uint64_t worker_concurrency;
  osrm::EngineConfig::Algorithm algorithm;

  config();
  config(json_t const& json);
};

}