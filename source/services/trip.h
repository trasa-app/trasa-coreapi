// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <aws/dynamodb/DynamoDBClient.h>

#include "rpc/service.h"
#include "routing/config.h"
#include "routing/osrm_interop.h"
#include "routing/scheduler.h"
#include "spacial/index.h"

namespace sentio::services
{

template <typename Impl>
class trip_service_base
  : public rpc::service_base
{
public:
  trip_service_base(
    routing::config config,
    spacial::index const& locator)
    : config_(std::move(config))
    , locator_(locator) {}

public:
  bool authenticated() const override { return true; }
  json_t invoke(json_t params, rpc::context ctx) const override
  { 
    return dynamic_cast<const Impl*>(this)->invoke(
      std::move(params), std::move(ctx)); 
  }

protected:
  routing::config const& config() const
  { return config_; }

  routing::scheduler const& scheduler() const
  { return scheduler_; }

  spacial::index const& locator() const
  { return locator_; }

private:
  routing::config config_;
  spacial::index locator_;
  routing::scheduler scheduler_;
};

class trip_service final
{
public:
  struct poll : public trip_service_base<poll>
  {
    using trip_service_base<poll>::trip_service_base;
    json_t invoke(json_t params, rpc::context ctx) const;
  };

  struct async : public trip_service_base<async>
  {
    using trip_service_base<async>::trip_service_base;
    json_t invoke(json_t params, rpc::context ctx) const;
  };

  struct sync : public trip_service_base<sync>
  {
    sync(
      routing::config config, 
      spacial::index const& locator,
      std::vector<import::region_paths> const& sources);

    using trip_service_base<sync>::trip_service_base;
    json_t invoke(json_t params, rpc::context ctx) const;

  private:
    routing::osrm_map instancesmap_;
  };
};

}