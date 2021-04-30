// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <boost/date_time.hpp>


using date_time_t = boost::posix_time::ptime;

std::string current_iso_datetime_string();