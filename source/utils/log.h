// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include "json.h"
#include <boost/log/trivial.hpp>

#define tracelog BOOST_LOG_TRIVIAL(trace)
#define dbglog BOOST_LOG_TRIVIAL(debug)
#define infolog BOOST_LOG_TRIVIAL(info)
#define warnlog BOOST_LOG_TRIVIAL(warning)
#define errlog BOOST_LOG_TRIVIAL(error)
#define fatallog BOOST_LOG_TRIVIAL(fatal)


namespace sentio::logging
{
  void init(json_t const& config);
}