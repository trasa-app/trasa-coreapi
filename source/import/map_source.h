// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <vector>
#include <future>
#include <optional>
#include <filesystem>

#include <boost/thread/future.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include "model/address.h"
#include "region_reader.h"

namespace sentio::import
{
/**
 * Represents a list of paths to data files that are 
 * needed to construct a region. This list os retreived 
 * from the config file.
 * 
 * The list can point either to a list of remote or local files.
 */
struct region_paths {
  std::string name;
  std::string addressbook;
  std::string poly;
  std::string osrm;

  /**
   * Reads the config file and picks the correct set of paths
   * based on the geocoder and routing configuration.
   */
  static std::vector<region_paths> from_config(
    json_t const& systemconfig);

  /**
   * Downloads remote paths locally to the system temp directory
   * returns a regions paths structure pointing to the local versions
   * of downloaded files.
   */
  static std::future<region_paths> download_async(
    region_paths const& remotepaths);
};

/**
 * Extracts download map osrm archives into temporary directories.
 */
std::vector<import::region_paths> extract_osrm_packages(
  std::vector<import::region_paths> const& sources);

/**
 * Extracts a single osrm tarbz2 archive into a set of osrm.* indecies
 * required by the osrm engine, and preprocessed by scripts in devops/ *
 */
std::string extract_map_archive(std::string path);

}  // namespace sentio::import