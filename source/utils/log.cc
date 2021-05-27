// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "log.h"
#include "json.h"

#include <atomic>
#include <iostream>
#include <functional>

#include <boost/ref.hpp>
#include <boost/bind.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/core/null_deleter.hpp>

#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/record_ordering.hpp>

namespace sentio::logging
{

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;


static std::atomic<size_t> thread_counter;

size_t assign_thread_id() {
    thread_local size_t tid = ++thread_counter;
    return tid;
}

void coloring_formatter(
  boost::log::record_view const& rec, 
  boost::log::formatting_ostream& strm)
{
  auto severity = rec[boost::log::trivial::severity];
  if (severity)
  {
      switch (severity.get())
      {
      case boost::log::trivial::severity_level::trace:
          strm << "\033[38;5;242m"; break;
      case boost::log::trivial::severity_level::debug:
          strm << "\033[38;5;246m"; break;
      case boost::log::trivial::severity_level::info:
          strm << "\033[32m"; break;
      case boost::log::trivial::severity_level::warning:
          strm << "\033[33m"; break;
      case boost::log::trivial::severity_level::error:
      case boost::log::trivial::severity_level::fatal:
          strm << "\033[31m"; break;
      default:
          break;
      }
  }
  strm << logging::extract<boost::posix_time::ptime>("ts", rec) << " | "
       << std::setw(5) << rec[logging::trivial::severity] << " | "
       << std::setw(2) << logging::extract<size_t>("tid", rec) << " | "
       << rec[expr::smessage];
  if (severity) {
      strm << "\033[0m";
  }
}

void init(json_t const&)
{
  typedef sinks::text_ostream_backend backend_t;
    typedef sinks::asynchronous_sink<
      backend_t,
      sinks::bounded_ordering_queue<
          logging::attribute_value_ordering<unsigned int, std::less<unsigned int>>,
          128,                        // queue no more than 128 log records
          sinks::block_on_overflow    // wait until records are processed
      >
    > sink_t;

  boost::shared_ptr<std::ostream> strm(
    &std::cout, boost::null_deleter());

  boost::shared_ptr<sink_t> sink(new sink_t(
    boost::make_shared<backend_t>(),
    keywords::order = logging::make_attr_ordering(
      "#", std::less<unsigned int>())));

  sink->locked_backend()
      ->add_stream(strm);
  sink->set_formatter(&coloring_formatter);

  // Add it to the core
  logging::core::get()->add_sink(sink);
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::trace);
  
  // Add some attributes too
  logging::core::get()->add_global_attribute("ts", attrs::local_clock());
  logging::core::get()->add_global_attribute("#", attrs::counter<unsigned int>());
}

}