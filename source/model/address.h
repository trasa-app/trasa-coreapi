// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <vector>
#include <boost/property_tree/ptree_fwd.hpp>
#include <aws/dynamodb/model/AttributeValue.h>

#include "utils/json.h"
#include "spacial/coords.h"

namespace sentio::model
{
/**
 * This type represents a single addressable building with its GPS coordinates.
 */
struct building 
{
  int64_t id;
  spacial::coordinates coords;
  std::string country;
  std::string city;
  std::string zipcode;
  std::string street;
  std::string number;

  static building from_json(json_t const&);
  json_t to_json() const;
};

struct address : public building 
{
  std::string suite;
};

/**
 * This type represents a unique street and all the buildings located on it.
 */
struct street {
  std::string name;
  std::string city;
  std::vector<building> buildings;
};

struct region {
  std::string name;
  std::string source;
  std::vector<spacial::coordinates> bounds;
};

}