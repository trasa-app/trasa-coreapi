// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include "model/address.h"
#include "utils/datetime.h"

namespace sentio::tracking
{
  
/**
 * This type records events that are triggered by
 * users in different locations. Examples of such
 * events are things like authentication, purchases
 * going through waypoints while being routed, etc.
 */
struct location_event {
  std::string accountid;
  date_time_t timestamp;
  spacial::coordinates location;
  std::string event_type;
  std::string event_params;
};

class location_log
{
public:
  location_log(std::string tablename);
  location_log(location_log&&);
  ~location_log();

public:
  void store_location_event(location_event event) const;
  void store_location_events(std::vector<location_event> events) const;

private:
  class impl;
  std::unique_ptr<impl> impl_;
};

}