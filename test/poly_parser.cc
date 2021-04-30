// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "catch.h"
#include "import/map_source.h"

#include <sstream>

TEST_CASE("Polyfile parser sanity check", "[polyfile]")
{
  using namespace sentio::model;
  using namespace sentio::import;
  std::stringstream ss;

  ss << " " << std::endl;
  ss << "województwo małopolskie" << std::endl;
  ss << "1" << std::endl;
  ss << " 19.5293411 49.5730542" << std::endl;
  ss << " 19.5183851 49.5734240" << std::endl;
  ss << " 19.5028861 49.5817613" << std::endl;
  ss << " 19.4804077 49.5870350" << std::endl;
  ss << " 19.4718653 49.6005735" << std::endl;
  ss << " 19.4673762 49.6137628" << std::endl;
  ss << " 19.4731000 49.6184268" << std::endl;
  ss << " 19.9722965 50.5162508" << std::endl;
  ss << "END" << std::endl;
  ss << "END" << std::endl;
  ss << " " << std::endl;

  region_sources::polygon p = to_polygon(ss, "województwo małopolskie");

  std::vector<coordinates> expected {
    coordinates(49.5730542, 19.5293411),
    coordinates(49.5734240, 19.5183851),
    coordinates(49.5817613, 19.5028861),
    coordinates(49.5870350, 19.4804077),
    coordinates(49.6005735, 19.4718653),
    coordinates(49.6137628, 19.4673762),
    coordinates(49.6184268, 19.4731000),
    coordinates(50.5162508, 19.9722965)
  };

  REQUIRE(p.outer() == expected);
}