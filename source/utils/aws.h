// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <boost/property_tree/ptree.hpp>

#include <aws/core/client/AWSClient.h>
#include <aws/dynamodb/model/AttributeValue.h>
#include <aws/dynamodb/model/AttributeDefinition.h>

namespace sentio::spacial
{
  class coordinates;
}

namespace sentio::aws
{
/**
 * Configures AWS resource names.
 * 
 * Those variables come from the main config
 * file and should be populated by terraform.
 */
struct config {
  std::string log_level;

  struct {
    std::string trips;
    std::string accounts;
    std::string locations;
  } tables;

  struct {
    std::string pending_routes;
  } queues;

  config(boost::property_tree::ptree const& json);
};

/**
 * Initialize the AWS SDK runtime. 
 * 
 * Must be called before any AWS related operations are 
 * performed, ideally first thing in main() after reading
 * config data.
 */
void init(config);

/**
 * The global AWS config.
 * 
 * This structure holds values that describe aws resources
 * created by terraform. They are retreived from the global
 * config file.
 */
config const& resources();


/**
 * Creates an AttributeValue shared pointer for use with
 * aws dynamodb api. This overload is for string values.
 */
template <typename ValT>
static auto av(ValT&& val)
{
  using Aws::DynamoDB::Model::AttributeValue;
  return std::make_shared<AttributeValue>(std::forward<ValT>(val));
}


Aws::DynamoDB::Model::AttributeValue as_av(spacial::coordinates const&);

/**
 * Creates an AttributeValue shared pointer for use with
 * aws dynamodb api. This overload is for non-string values.
 */
std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> av(bool val);
std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> av(double val);
}