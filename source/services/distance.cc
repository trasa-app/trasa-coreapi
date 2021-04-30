// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "distance.h"
#include "rpc/error.h"
#include "spacial/coords.h"

namespace sentio::services 
{

distance_service::distance_service(
  routing::config const& config,
  spacial::index const& index,
  std::vector<import::region_paths> const& sources)
  : index_(index)
  , instancesmap_(config, sources) { }

json_t distance_service::invoke(json_t params, rpc::context) const 
{
  spacial::coordinates to(params.get_child("to"));
  spacial::coordinates from(params.get_child("from"));
  
  auto to_region = index_.locate(to);
  auto from_region = index_.locate(from);

  if (!to_region || !from_region) {
    throw rpc::bad_request("region not found");
  }

  if (to_region != from_region) {
    throw rpc::bad_request("cross region routing not supported");
  }

  routing::travel_cost cost = instancesmap_
    .calculate_distance(
      from, to, to_region->name());

  json_t output;
  output.add("meters", cost.distance);
  output.add("seconds", cost.duration.count());
  return output;
}

}