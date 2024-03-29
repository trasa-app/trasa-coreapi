// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <list>
#include <string>
#include <iostream>
#include <filesystem>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "rpc/web.h"
#include "rpc/server.h"

#include "spacial/index.h"
#include "import/map_source.h"

#include "routing/worker.h"

#include "services/trip.h"
#include "services/distance.h"
#include "services/geocoder.h"

#include "utils/aws.h"
#include "utils/log.h"
#include "utils/future.h"

struct exec_role {
  using type = const char*;
  static constexpr type none = "none";
  static constexpr type rpc = "rpc";
  static constexpr type worker = "worker";
  static constexpr type both = "both";
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

exec_role::type read_role(int argc, const char** argv)
{
  if (argc == 3) {
    if (boost::iequals(argv[2], exec_role::rpc)) {
      return exec_role::rpc;
    } else if (boost::iequals(argv[2], exec_role::worker)) {
      return exec_role::worker;
    } else if (boost::iequals(argv[2], exec_role::both)) {
      return exec_role::both;
    } else if (boost::iequals(argv[2], exec_role::none)) {
      return exec_role::none;
    } else {
      throw std::invalid_argument(
        "unrecognized role (expected rpc, worker or both)");
    }
  }
  return exec_role::none; // default
}


sentio::rpc::service_map_t create_services(
  json_t const& systemconfig,
  sentio::spacial::index const& worldix,
  std::vector<sentio::import::region_paths> const& sources)
{
  using namespace sentio::rpc;
  using namespace sentio::services;
  

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

  return svcmap;
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

  BOOST_LOG_SCOPED_THREAD_TAG("tid", 
    sentio::logging::assign_thread_id());

  try {
    // this holds rpc config, regions config, aws, etc.
    boost::property_tree::ptree systemconfig;
    boost::property_tree::read_json(argv[1], systemconfig);

    // initialize system logging
    sentio::logging::init(systemconfig.get_child("logging"));

    // check how we are going to start this instance of
    // the executable.
    exec_role::type role = read_role(argc, argv);
    infolog << "running in role: " << role;

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

    if (role == exec_role::none) {
      return 0;
    }

    sentio::spacial::index worldix(sources);
    auto services = create_services(
      systemconfig, worldix, extracted_sources);

    // this is the set of configs needed to expose JSON-RPC endpoints over http.
    sentio::rpc::config rpcconfig{
      .listen_ip = systemconfig.get<std::string>("rpc.address"),
      .listen_port = systemconfig.get<uint16_t>("rpc.port"),
      .guard = systemconfig.get_child("rpc.auth")};

    // depending on the enabled roles for this instance, this
    // will have either one or two main threads, each for one
    // of the roles.
    std::list<std::thread> rolethreads;

    if (role == exec_role::rpc || role == exec_role::both) {
      // this means that this node will respond 
      // to JSON-RPC requests over WebSockets.
      rolethreads.emplace_back([&rpcconfig, &services]() {
        BOOST_LOG_SCOPED_THREAD_TAG("tid", 
          sentio::logging::assign_thread_id());
        infolog << "starting web rpc role";
        sentio::rpc::web_server(rpcconfig, services).start();
      });
    }

    if (role == exec_role::worker || role == exec_role::both) {
      infolog << "starting sqs worker role";
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
