// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "log.h"
#include "json.h"

#include <iostream>
#include <boost/core/null_deleter.hpp>

#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>


namespace sentio::logging
{

void init(json_t const& config)
{
  using namespace boost::log;
  namespace expr = boost::log::expressions; 

  auto core = core::get();
  auto backend = boost::make_shared<sinks::text_ostream_backend>();

  backend->add_stream(boost::shared_ptr<std::ostream>(
    &std::clog, boost::null_deleter()));
  backend->auto_flush(config.get<bool>("dev"));

  auto sink = boost::make_shared<
    sinks::synchronous_sink<
      sinks::text_ostream_backend
    >>(backend);
  core->add_sink(sink);
}

}