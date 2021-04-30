// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "datetime.h"

std::string current_iso_datetime_string()
{ 
  return boost::posix_time::to_iso_string(
    boost::posix_time::second_clock::universal_time()) + "Z";
}