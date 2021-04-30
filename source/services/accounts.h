// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include "rpc/service.h"
#include "spacial/index.h"
#include "account/store.h"
#include "tracking/event.h"

namespace sentio::services
{

struct auth_config {
  std::string secret;
  uint64_t token_ttl_seconds;

  auth_config(json_t const& json);
};

class checkin_service final 
  : public rpc::service_base
{
public:
  checkin_service(
    spacial::index const& index, 
    auth_config config);

public:
  bool authenticated() const override;
  json_t invoke(
    json_t params, 
    rpc::context ctx) const override;

private:
  spacial::index const& index_;
  tracking::location_log loclog_;
  account::repository repo_;
  auth_config config_;
};

}