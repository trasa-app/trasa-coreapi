// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <list>
#include <string>
#include <iostream>
#include <filesystem>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/core/null_deleter.hpp>

// #include <boost/log/expressions.hpp>
// #include <boost/log/attributes.hpp>
// #include <boost/log/sinks/async_frontend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>

#include "rpc/server.h"
#include "spacial/index.h"
#include "import/map_source.h"

#include "routing/worker.h"

#include "services/trip.h"
#include "services/distance.h"
#include "services/geocoder.h"
#include "services/accounts.h"

#include "utils/aws.h"
#include "utils/log.h"
#include "utils/future.h"

enum exec_role {
  none    = 0x00,
  rpc     = 0x01, 
  worker  = 0x02, 
  both = rpc | worker,
};

auto download_regions(std::vector<sentio::import::region_paths> const& sources)
{
  using namespace sentio::import;
  std::vector<std::future<region_paths>> import_tasks;
  
  for (auto const& region_paths : sources) {
    import_tasks.emplace_back(
      region_paths::download_async(
        region_paths));
  }

  sentio::utils::wait_for_all(
    import_tasks.begin(), 
    import_tasks.end());

  std::vector<region_paths> output;
  for (auto& task : import_tasks) {
    output.emplace_back(task.get());
  }
  return output;
}

exec_role read_role(int argc, const char** argv)
{
  if (argc == 3) {
    if (boost::iequals(argv[2], "rpc")) {
      return exec_role::rpc;
    } else if (boost::iequals(argv[2], "worker")) {
      return exec_role::worker;
    } else if (boost::iequals(argv[2], "both")) {
      return exec_role::both;
    } else if (boost::iequals(argv[2], "none")) {
      return exec_role::none;
    } else {
      throw std::invalid_argument(
        "unrecognized role (expected rpc, worker or both)");
    }
  }
  return exec_role::none; // default
}

std::thread start_rpc_server(
  boost::property_tree::ptree const& systemconfig,
  std::vector<sentio::import::region_paths> const& sources)
{
  using namespace sentio::rpc;
  using namespace sentio::services;

  return std::thread([&]{
    sentio::spacial::index worldix(sources);
    // this is the set of configs needed to expose JSON-RPC endpoints over http.
    sentio::rpc::config rpcconfig{
      .listen_ip = systemconfig.get<std::string>("rpc.address"),
      .listen_port = systemconfig.get<uint16_t>("rpc.port"),
      .secret = systemconfig.get<std::string>("authentication.secret")};
    sentio::rpc::service_map_t svcmap;

    // svcmap.emplace("trip.poll",   
    //   create_service(trip_service::poll(
    //     systemconfig.get_child("routing"), worldix)));

    // svcmap.emplace("trip.async",
    //   create_service(trip_service::async(
    //     systemconfig.get_child("routing"), worldix)));

    svcmap.emplace("trip",
      create_service(trip_service::sync(
        systemconfig.get_child("routing"), worldix, sources)));
    
    svcmap.emplace("geocode", 
      create_service(geocoder_service(worldix, sources,
        systemconfig.get_child("geocoder"))));
    
    svcmap.emplace("distance", 
      create_service(distance_service(
        systemconfig.get_child("routing"), worldix, sources)));

    // this blocks the current thread until the server terminates
    sentio::rpc::run_server(
      std::move(rpcconfig), 
      std::move(svcmap));
  });
}


std::thread start_worker_server(
  boost::property_tree::ptree const& systemconfig,
  std::vector<sentio::import::region_paths> const& sources)
{
  return std::thread([&]{
    infolog << "starting routing worker.";
    sentio::routing::start_routing_worker(
      systemconfig.get_child("routing"), sources);
  });
}

void init_logging()
{
  using namespace boost::log;
  namespace expr = boost::log::expressions; 

  auto core = core::get();
  auto backend = boost::make_shared<sinks::text_ostream_backend>();

  backend->add_stream(boost::shared_ptr<std::ostream>(
    &std::clog, boost::null_deleter()));
  backend->auto_flush(true);

  auto sink = boost::make_shared<
    sinks::synchronous_sink<
      sinks::text_ostream_backend
    >>(backend);
  core->add_sink(sink);
}

int main(int argc, const char** argv)
{
  using namespace sentio::utils;
  using namespace sentio::import;

  if (argc < 2 || argc > 3) {
    std::cerr << "usage: " << argv[0] 
              << " <config-file> [rpc|worker|both]"
              << std::endl;
    return 1;
  }

  init_logging();

  try {
    // check how we are going to start this instance of
    // the executable.
    exec_role role = read_role(argc, argv);
    infolog << "running in role: " << role;

    // this holds rpc config, regions config, aws, etc.
    boost::property_tree::ptree systemconfig;
    boost::property_tree::read_json(argv[1], systemconfig);

    // initialize aws api and configure resource names
    sentio::aws::init(systemconfig.get_child("aws"));

    // read all enabled regions and download their map data
    // from the storage server. The `sources` collection will
    // have paths to the locally stored downloaded files.
    std::vector<region_paths> sources = download_regions(
      region_paths::from_config(systemconfig));

    infolog << "downloaded " << sources.size() << " regions.";
    dbglog << "extracting " << sources.size() << " regions....";

    auto extracted_sources = extract_osrm_packages(sources);
    infolog << "extracted " << extracted_sources.size() << " regions";

    // depending on the enabled roles for this instance, this
    // will have either one or two main threads, each for one
    // of the roles.
    std::list<std::thread> rolethreads;

    if (role & exec_role::rpc) {
      infolog << "starting rpc role";
      // this means that this node will respond to RPC 
      // requests over HTTP.
      rolethreads.emplace_back(
        start_rpc_server(systemconfig, extracted_sources));
    }

    if (role & exec_role::worker) {
      infolog << "starting worker role";
      // this means that this node will poll the work queue
      // for outstanding trip optimization requests and compute
      // optimized routes then store them in dynamodb.
      // rolethreads.emplace_back(
      rolethreads.emplace_back(
        start_worker_server(systemconfig, extracted_sources));
    }

    // this should block forever 
    for (auto& t: rolethreads) {
      t.join();
    }
    
  } catch (std::exception const& e) {
    errlog << "fatal: " << e.what();
    throw;
  }
  return 0;
}
