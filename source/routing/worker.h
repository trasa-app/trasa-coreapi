// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include "config.h"
#include "import/map_source.h"

namespace sentio::routing
{
  void start_routing_worker(config const&, 
    std::vector<import::region_paths> const&);
}
