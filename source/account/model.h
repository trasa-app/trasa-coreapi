// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <vector>
#include <optional>

#include "utils/datetime.h"

namespace sentio::account
{

/**
 * This type represents a reason or condition under which
 * an account would be denied authentication. This may include
 * time-based restrictions, location-based restriction, or
 * custom restrictions placed by system administrators.
 */
struct restriction {
  /**
   * If true, then this restriction will be evaluated.
   * Otherwise, it will be ignored, regardless if the underlying
   * condition is effecive.
   */
  bool enabled;

  /**
   * A human readable reason that explains why an account is restricted.
   */
  std::string reason;

  /**
   * An string identifier of the type of the restriction.
   * 
   * Internally this maps to a function that takes the params 
   * value and returns true if the restriction is effective, 
   * otherwise false.
   */
  std::string type;

  /**
   * Parameters to the restriction evaluation function.
   * The format of this value is implementation defined and 
   * depends on the type of the restriction.
   */
  std::string params;

  /**
   * When this restriction was placed on the account.
   */
  date_time_t created_at;
};


enum class platform_type { ios, android };
const char* to_string(platform_type);
platform_type from_string(std::string const&);

/**
 * This type represents a single device that was used by
 * an account. One account may be used accross multiple devices
 * and sometimes a software update may appear as a different device.
 */
struct device {
  /**
   * Eithe iOS or Android are supported atm.
   */
  platform_type platform;

  /**
   * The operating system version currently installed on the device.
   */
  std::string os_version;

  /**
   * A string that identifies the physical hardware running
   * the app on the client, e.g "iPhone11,1"
   */
  std::string hardware_model;

  /**
   * Stores when the account used this device for the first time.
   */
  date_time_t first_use;
};

/**
 * This type represents a unique user identity in the system.
 *
 * In practice this translates to a unique Apple AppStoreID or
 * Google PlayStore ID, in form of an anonymized guid or hash.
 *
 * An account is the entity that holds user balance, is used to
 * aggregate statistics about a driver, etc.
 *
 * One account belongs to one or more devices using the same
 * store account.
 */
struct account {
  /**
   * An unique identifier that addresses accounts. This identifier
   * is derived from Apple AppStore id or Google PlayStore id.
   */
  std::string uid;

  /**
   * Stores a list of all known devices used to access this account.
   */
  std::vector<device> devices;

  /**
   * A set of restrictions that could potentially deny a an account
   * entry to the system. This could include things such as time-based
   * restrictions, or IP-based restrictions (for dev accounts), etc.
   *
   * If any of the restrictions object in this collection returns
   * true for both enabled() and effective(), then entry is denied.
   */
  std::vector<restriction> restrictions;

  /**
   * Timestamp of the first recorded authentication request with this uid.
   */
  date_time_t created_at;
};

}

namespace std
{

/**
 * Used to quickly uniquely identify a device and speed up search
 * for exisiting devices with the same "device signature"
 */
template <>
struct hash<sentio::account::device> {
  size_t operator()(sentio::account::device const& device) const;
};

/**
 * Used to order devices attached to an account by their
 * first usage timestamp.
 */
template <>
struct less<sentio::account::device> {
  bool operator()(sentio::account::device const& left,
                  sentio::account::device const& right) const;
};

template <>
struct equal_to<sentio::account::device> {
  bool operator()(sentio::account::device const& left,
                  sentio::account::device const& right) const;
};

} 