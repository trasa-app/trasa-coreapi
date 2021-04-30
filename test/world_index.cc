// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <vector>
#include <string>
#include <iterator>

#include "catch.h"
#include "mock_data.h"
#include "index/world.h"
#include "index/region.h"

TEST_CASE("World Index Sanity Check - positive cases", "[world]")
{
  using coordinates = sentio::spacial::coordinates;

  auto podix = make_region_test_index("podlaskie", podlaskie_bounds);
  auto pomix = make_region_test_index("pomorskie", pomorskie_bounds);

  sentio::index::world worldix;
  worldix.insert(std::move(podix));
  worldix.insert(std::move(pomix));

  REQUIRE(worldix.size() == 2);

  auto it = worldix.begin();
  REQUIRE(it->name() == "podlaskie");

  auto it2 = std::next(it);
  REQUIRE(it2->name() == "pomorskie");

  auto podit = worldix.locate(coordinates{53.135278, 23.145556});
  REQUIRE(podit != worldix.end());
  REQUIRE(podit->name() == "podlaskie");

  auto pomit = worldix.locate(coordinates{54.350823, 18.665475});
  REQUIRE(pomit != worldix.end());
  REQUIRE(pomit->name() == "pomorskie");

  auto errit = worldix.locate(coordinates{64.350823, 28.665475});
  REQUIRE(errit == worldix.end());
}