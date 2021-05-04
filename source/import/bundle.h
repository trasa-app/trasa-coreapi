// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>

namespace sentio::import 
{

/**
 * Represents a list of paths to data files that are 
 * needed to construct a region. This list os retreived 
 * from the config file.
 * 
 * The list can point either to a list of remote or local files.
 */
class region_bundle {
public:
  region_bundle() {}

private:
  std::string name_;
};

}