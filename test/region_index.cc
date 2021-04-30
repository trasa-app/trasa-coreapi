// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>

#include "catch.h"
#include "mock_data.h"
#include "index/region.h"

TEST_CASE("Region Index Sanity Check - positive cases", "[region]")
{
  using namespace sentio::model;
  using namespace sentio::index;

  auto ix = make_region_test_index(
    "podlaskie", podlaskie_bounds);
 
  for (size_t i = 0; i < _countof(buildings2); ++i) {
    ix.insert(buildings2[i]);
  }

  REQUIRE(ix.size() == 5);
  auto sit = ix.find("Wiejska");

  REQUIRE(sit != ix.end());

  REQUIRE(sit->second.size() == 2);  // Białystok & Suwałki
  REQUIRE(sit->second.find("Suwałki") != sit->second.end());
  REQUIRE(sit->second.find("Białystok") != sit->second.end());
  REQUIRE(sit->second.find("Białystok")->second.name() == "Wiejska");

  auto search_result = sit->second.find("Białystok")->second.find("35C");
  REQUIRE(search_result != sit->second.find("Białystok")->second.end());
  REQUIRE(search_result->second.latitude() == 7);
  REQUIRE(search_result->second.longitude() == 8);

  // 53.135278, 23.145556 == bialystok
  REQUIRE(ix.contains(coordinates{53.135278, 23.145556}) == true);
  REQUIRE(ix.contains(coordinates{63.135278, 23.145556}) == false);
}