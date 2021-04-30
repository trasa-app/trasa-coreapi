// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <optional>
#include <boost/algorithm/string.hpp>

#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>

#include "aws.h"
#include "spacial/coords.h"

namespace sentio::aws
{
bool g_aws_initialized = false;
std::optional<config> g_aws_config;

config::config(boost::property_tree::ptree const& json)
    : log_level(json.get<std::string>("log_level"))
    , tables{.trips = json.get<std::string>("tables.trips"),
             .accounts = json.get<std::string>("tables.accounts"),
             .locations = json.get<std::string>("tables.locations")}
    , queues{.pending_routes = json.get<std::string>("queues.pending_routes")}
{
}

Aws::Utils::Logging::LogLevel level_from_string(std::string const& val)
{
  using namespace Aws::Utils::Logging;
  for (int i = 0; i <= (int)LogLevel::Trace; ++i) {
    auto levelval = static_cast<LogLevel>(i);
    auto levelname = GetLogLevelName(levelval);
    if (boost::iequals(levelname, val)) {
      return levelval;
    }
  }

  // default log level
  return LogLevel::Warn;
}

void init(config cfg)
{
  if (g_aws_initialized) {
    throw std::runtime_error("aws already initialized");
  }

  Aws::SDKOptions options;
  options.loggingOptions.logLevel = level_from_string(cfg.log_level);
  options.httpOptions.installSigPipeHandler = true;
  Aws::InitAPI(options);

  g_aws_config = g_aws_config.emplace(std::move(cfg));
  g_aws_initialized = true;
}

config const& resources()
{
  if (!g_aws_config.has_value() || !g_aws_initialized) {
    throw std::runtime_error("aws not initialized");
  }
  return g_aws_config.value();
}

Aws::DynamoDB::Model::AttributeValue as_av(spacial::coordinates const& coords)
{
  return Aws::DynamoDB::Model::AttributeValue()
      .AddMEntry("latitude", av(coords.latitude()))
      .AddMEntry("longitude", av(coords.longitude()));
}

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> av(bool val)
{
  using Aws::DynamoDB::Model::AttributeValue;
  auto attrib = std::make_shared<AttributeValue>();
  attrib->SetBool(val);
  return attrib;
}

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> av(double val)
{
  using Aws::DynamoDB::Model::AttributeValue;
  auto attrib = std::make_shared<AttributeValue>();
  attrib->SetN(val);
  return attrib;
}


}  // namespace sentio::aws

