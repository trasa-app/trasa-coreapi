// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <cstdlib>
#include <type_traits>

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/DynamoDBRequest.h>

#include <boost/core/ignore_unused.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "accounts.h" 
#include "rpc/jwt.h"
#include "rpc/error.h"
#include "utils/aws.h"
#include "utils/meta.h"
#include "account/model.h"
#include "account/store.h"
#include "model/address.h"
#include "tracking/event.h"

namespace sentio::services
{

auth_config::auth_config(boost::property_tree::ptree const& ptree)
  : secret(ptree.get<std::string>("secret"))
  , token_ttl_seconds(ptree.get<uint64_t>("token_ttl"))
{
}

class authenticate_request
{
public:
  authenticate_request(json_t json)
      : reqjson_(std::move(json))
  {
    verify_argument(reqjson_.get_child_optional("creds.key"));
    verify_argument(reqjson_.get_child_optional("creds.nbf"));

    verify_argument(reqjson_.get_child_optional("device.uid"));
    verify_argument(reqjson_.get_child_optional("device.model"));
    verify_argument(reqjson_.get_child_optional("device.platform"));
    verify_argument(reqjson_.get_child_optional("device.osversion"));
    verify_argument(reqjson_.get_child_optional("device.appversion"));

    verify_argument(reqjson_.get_child_optional("location.latitude"));
    verify_argument(reqjson_.get_child_optional("location.longitude"));
  }

  std::string uid() const { return reqjson_.get<std::string>("device.uid"); }

  spacial::coordinates coords() const
  {
    return spacial::coordinates {
      reqjson_.get<double>("location.latitude"),
      reqjson_.get<double>("location.longitude")
    };
  }

  account::device device() const
  {
    auto const& platstring = reqjson_.get<std::string>("device.platform");
    return account::device {
      .platform = account::from_string(platstring),
      .os_version = reqjson_.get<std::string>("device.osversion"),
      .hardware_model = reqjson_.get<std::string>("device.model"),
      .first_use = boost::posix_time::second_clock::universal_time()
    };
  }

private:
  json_t reqjson_;
};

account::account record_devices_change(
  account::repository const& repo,
  account::account account,
  account::device device)
{
  // check if there are any changes to the devices list
  std::hash<account::device> dhasher;
  auto this_device_hash = dhasher(device);
  bool matched = false;
  for (auto const& knowndevice : account.devices) {
    if (dhasher(knowndevice) == this_device_hash) {
      matched = true;
      break;
    }
  }
  if (!matched) {
    // this is the first time we're seeing this device (or this configuration)
    // This might happen also when the user updates their OS version, etc.
    account.devices.emplace_back(std::move(device));
    account = repo.upsert(std::move(account));
  }

  return account;
}

checkin_service::checkin_service(
  spacial::index const& index, 
  auth_config config)
    : index_(index)
    , loclog_(sentio::aws::resources().tables.locations)
    , repo_(sentio::aws::resources().tables.accounts)
    , config_(std::move(config))
{
}

/**
 * Tells the RPC server handler that this service does not
 * need the client to be authenticated when calling it.
 */
bool checkin_service::authenticated() const { return false; }

json_t checkin_service::invoke(json_t params, rpc::context) const
{
  using namespace boost::posix_time;
  std::optional<authenticate_request> request;
  try {
    request.emplace(std::move(params));
  } catch (std::exception const& e) {
    throw rpc::bad_request(e.what());
  }

  tracking::location_event locev{
    .accountid = request->uid(),
    .timestamp = second_clock::universal_time(),
    .location = request->coords(),
    .event_type = "authentication",
    .event_params = ""
  };

  // record authentication request with location
  if (!index_.locate(request->coords()).has_value()) {
    locev.event_params = "fail:region_not_supported";
    loclog_.store_location_event(std::move(locev));
    throw rpc::not_authorized("location not supported");
  }

  bool first_use = false;
  auto account = repo_.retrieve(request->uid());
  if (!account.has_value()) {  // first time, then signup
    first_use = true;
    try {
      account = repo_.upsert(
          account::account {
            .uid = request->uid(),
            .devices = {request->device()},
            .restrictions = {},
            .created_at = second_clock::universal_time()
          });
      locev.event_params = "ok:signup";
    } catch (...) {
      locev.event_params = "fail:signup_error";
      loclog_.store_location_event(std::move(locev));
      throw;
    }
  } else {  // known account, just check if devices changed
    // store any changes to the device used by this account.
    locev.event_params = "ok:normal";

    account = record_devices_change(repo_, *account, request->device());
  }

  // todo: implement check restrictions

  // issue token
  boost::property_tree::ptree payload;
  auto token_exp = second_clock::universal_time() + 
    seconds(config_.token_ttl_seconds);

  payload.put("exp", to_iso_string(token_exp) + 'Z'); // +Z makes it UTC
  payload.put("uid", account->uid);
  std::stringstream tokenss;
  boost::property_tree::write_json(tokenss, payload, false);

  json_t output;
  output.add("uid", account->uid);
  output.add("expires", payload.get<std::string>("exp"));
  output.add("token", rpc::jwt::issue_token(tokenss.str(), config_.secret));

  if (first_use) {
    output.add("new", true);
  }

  loclog_.store_location_event(std::move(locev));
  return output;
}

}  // namespace sentio::kurier