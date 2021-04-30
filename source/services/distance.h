// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include "rpc/service.h"
#include "spacial/index.h"
#include "routing/osrm_interop.h"

namespace sentio::services 
{

class distance_service final 
  : public rpc::service_base
{
public:
  distance_service(
    routing::config const& config,
    spacial::index const& index,
    std::vector<import::region_paths> const& sources);

public:
  json_t invoke(
    json_t params, 
    rpc::context ctx) const;

private:
  spacial::index const& index_;
  routing::osrm_map instancesmap_;
};

}