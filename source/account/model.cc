// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "model.h"
#include <boost/functional/hash.hpp>

namespace sentio::account
{

/**
 * Converts Platform enum type to string, for use in
 * dynamodb storage and debugging.
 */
const char* to_string(platform_type p)
{
  if (p == platform_type::android) {
    return "android";
  } else if (p == platform_type::ios) {
    return "ios";
  } else {
    throw std::invalid_argument("unrecognized platform");
  }
}

/**
 * Interprets a platform identifier string as an enum.
 * Used when reading from dynamodb or json data.
 */
platform_type from_string(std::string const& s)
{
  if (boost::iequals(s, "android")) {
    return platform_type::android;
  } else if (boost::iequals(s, "ios")) {
    return platform_type::ios;
  } else {
    throw std::invalid_argument("unrecognized platform: " + s);
  }
}

}

namespace std
{

size_t hash<sentio::account::device>::operator()(
    sentio::account::device const& device) const
{
  size_t seed = 1337;
  boost::hash_combine(seed, device.platform);
  boost::hash_combine(seed, device.os_version);
  boost::hash_combine(seed, device.hardware_model);
  return seed;
}

bool equal_to<sentio::account::device>::operator()(
    sentio::account::device const& left, 
    sentio::account::device const& right) const
{
  return left.platform == right.platform &&
         left.hardware_model == right.hardware_model &&
         left.os_version == right.os_version &&
         left.first_use == right.first_use;
}

}