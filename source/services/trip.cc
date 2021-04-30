// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <random>
#include <optional>

#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <aws/dynamodb/model/AttributeValue.h>

#include "trip.h"
#include "rpc/error.h"
#include "routing/trip.h"
#include "utils/aws.h"
#include "utils/datetime.h"
#include "spacial/coords.h"

namespace sentio::services
{

static std::string random_string(size_t len)
{
  static std::string chars(
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  static std::random_device rd;
  static std::mt19937 mt(rd());

  std::string output(len, '0');
  std::uniform_int_distribution<size_t> dist(0, chars.size() - 1);
  std::generate_n(output.begin(), len, [&]() { 
    return chars[dist(mt)]; 
  });
  return output;
}

json_t trip_service::poll::invoke(json_t params, rpc::context ctx) const 
{ 
  using namespace Aws::DynamoDB::Model;
  boost::ignore_unused(ctx);

  Aws::DynamoDB::DynamoDBClient dbclient;
  auto tripid = params.get_optional<std::string>("tripid");
  if (!tripid.has_value() || tripid.value().empty()) {
    throw rpc::bad_request("missing parameter");
  }

  GetItemRequest girequest;
  girequest.SetTableName(aws::resources().tables.trips);
  girequest.AddKey("id", AttributeValue(tripid.value()));
  auto giresponse = dbclient.GetItem(girequest);

  if (!giresponse.IsSuccess()) {
    std::cerr << "trip.poll failed for tripid " << tripid.value() 
              << " with error: " << giresponse.GetError().GetMessage()
              << std::endl;
    throw rpc::server_error();
  }

  // not ready yet, invalid or whatever. Just tell the
  // client that the route is not ready.
  if (giresponse.GetResult().GetItem().size() == 0) {
    json_t output;
    output.add("id", tripid.value());
    output.add("status", "pending");
    return output;
  }

  auto const& item = giresponse.GetResult().GetItem();
  if (!item.contains("accountid")) {
    std::cerr << "trip.poll failed for tripid " << tripid.value() 
              << " because it doesn't have an accountid associated"
              << std::endl;
    throw rpc::server_error();
  }

  if (!item.contains("status")) {
    std::cerr << "trip.poll failed for tripid " << tripid.value() 
              << " because it doesn't have a valid status value"
              << std::endl;
  }

  auto const& accountidval = item.find("accountid")->second;
  if (!boost::iequals(ctx.uid.value(), accountidval.GetS())) {
    throw rpc::not_authorized();
  }

  if (item.find("status")->second.GetS() == "ready") {
    std::stringstream ss;
    ss.write(
      item.find("response")->second.GetS().c_str(), 
      item.find("response")->second.GetS().size());

    json_t output, result;
    boost::property_tree::read_json(ss, result);

    output.add("id", item.find("id")->second.GetS());
    output.add("status", "ready");
    output.add("duration", item.find("duration")->second.GetN());
    output.add("distance", item.find("distance")->second.GetN());
    output.add("region", item.find("region")->second.GetS());
    output.add("timestamp", item.find("timestamp")->second.GetS());
    output.add_child("result", std::move(result));

    return output; 
  } else {
    json_t output;
    output.add("id", item.find("id")->second.GetS());
    output.add("status", item.find("status")->second.GetS());
    return output;
  }
}

// trip.async implementation

json_t trip_service::async::invoke(json_t params, rpc::context ctx) const 
{ 
  std::optional<routing::trip_request> request;
  spacial::coordinates creation_location(
    params.get_child("location"));
  auto tripregion = locator().locate(creation_location);

  try {
    params.add("meta.accountid", ctx.uid.value());
    params.add("meta.create_at", current_iso_datetime_string());
    params.add("meta.region", tripregion->name());
    request.emplace(std::move(params));
  } catch (std::exception const& e) {
    throw rpc::bad_request(e.what());
  }

  if (request->trip().size() > config().max_waypoints) {
    std::cerr << "trip request contains " << request->trip().size() 
              << " waypoints, configured maximum is "
              << config().max_waypoints << ". aborting." << std::endl;
    throw std::invalid_argument("trip too large");
  }

  // cross-regional routes are not supported yet.
  // reject all trips that have waypoints not belonging
  // to the region of the trip.
  for (auto const& waypoint: request->trip()) {
    if (locator().locate(waypoint.building.coords) != tripregion) {
      std::stringstream ss;
      boost::property_tree::write_json(ss, waypoint.to_json());
      std::cerr << "waypoint " << ss.str() 
                << " is not within trip region " 
                << tripregion->name() << std::endl;
      throw std::invalid_argument("waypoint not within region");
    }
  }

  json_t output;
  auto promise = scheduler().schedule_trip(std::move(*request));
  output.add("trip.state", "pending");
  output.add("trip.mode", "asynchronous");
  output.add_child("trip.promise", promise.to_json());
  return output; 
}

trip_service::sync::sync(
  routing::config config, 
  spacial::index const& locator,
  std::vector<sentio::import::region_paths> const& sources)
  : trip_service_base(std::move(config), locator)
  , instancesmap_(config, sources)
{
}

json_t trip_service::sync::invoke(json_t params, rpc::context ctx) const 
{
  std::optional<routing::trip_request> request;
  spacial::coordinates creation_location(
    params.get_child("location"));
  auto tripregion = locator().locate(creation_location);

  try {
    params.add("meta.id", "s_" + random_string(16));
    params.add("meta.accountid", ctx.uid.value());
    params.add("meta.createdat", current_iso_datetime_string());
    params.add("meta.region", tripregion->name());
    request.emplace(std::move(params));
  } catch (std::exception const& e) {
    std::cerr << "trip parsing failed: " << e.what() << std::endl;
    throw rpc::bad_request(e.what());
  }

  if (request->trip().size() > config().max_waypoints) {
    std::cerr << "trip request contains " << request->trip().size() 
              << " waypoints, configured maximum is "
              << config().max_waypoints << ". aborting." << std::endl;
    throw std::invalid_argument("trip too large");
  }

  // cross-regional routes are not supported yet.
  // reject all trips that have waypoints not belonging
  // to the region of the trip.
  for (auto const& waypoint: request->trip()) {
    if (locator().locate(waypoint.building.coords) != tripregion) {
      std::stringstream ss;
      boost::property_tree::write_json(ss, waypoint.to_json());
      std::cerr << "waypoint " << ss.str() 
                << " is not within trip region " 
                << tripregion->name() << std::endl;
      throw std::invalid_argument("waypoint not within region");
    }
  }
  auto optimized = instancesmap_.optimize_trip(
    request->trip(), request->meta().region());
  return optimized.to_json();
}

}