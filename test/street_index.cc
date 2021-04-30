// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <vector>
#include <string>
#include <iterator>

#include "catch.h"
#include "mock_data.h"
#include "index/street.h"

TEST_CASE("Street Index Sanity Check - positive cases", "[street]")
{
  sentio::index::street ix("Wiejska");

  ix.insert(buildings[0]);
  ix.insert(buildings[1]);
  ix.insert(buildings[2]);

  REQUIRE(ix.size() == 3);
  REQUIRE(ix.name() == "Wiejska");

  std::vector<std::string> numbers1;
  for (auto b : ix) {
    numbers1.push_back(b.first);
  }

  REQUIRE(numbers1.size() == 3);
  REQUIRE(numbers1[0] == "18");
  REQUIRE(numbers1[1] == "35C");
  REQUIRE(numbers1[2] == "35D");

  std::vector<std::string> numbers2;
  auto matches2 = ix.prefix_match("35");
  for (auto it = matches2.first; it != matches2.second; ++it) {
    numbers2.push_back(it->first);
  }

  REQUIRE(numbers2.size() == 2);
  REQUIRE(numbers2[0] == "35C");
  REQUIRE(numbers2[1] == "35D");

  std::vector<std::string> numbers3;
  auto matches3 = ix.prefix_match("3");
  for (auto it = matches3.first; it != matches3.second; ++it) {
    numbers3.push_back(it->first);
  }

  REQUIRE(numbers3.size() == 2);
  REQUIRE(numbers3[0] == "35C");
  REQUIRE(numbers3[1] == "35D");

  auto matches4 = ix.prefix_match("1");
  REQUIRE(std::distance(matches4.first, matches4.second) == 1);
  REQUIRE(matches4.first->first == "18");

  auto b = ix.find("18");

  REQUIRE(b != ix.end());
  REQUIRE(b->second.latitude() == 5);
  REQUIRE(b->second.longitude() == 6);
}

TEST_CASE("Street Index Sanity Check - negative cases", "[street]")
{
  sentio::index::street ix1("Pogodna");

  // empty coords
  sentio::model::building b_no_coords{
      .id = 1,
      .coords = sentio::spacial::coordinates(0, 0),
      .country = "PL",
      .city = "Białystok",
      .zipcode = "15-370",
      .street = "Pogodna",
      .number = "35C"};

  REQUIRE_THROWS_AS(ix1.insert(b_no_coords), std::invalid_argument);

  // building belongs to a different street
  sentio::model::building b_different_street{
      .id = 1,
      .coords = sentio::spacial::coordinates(0, 0),
      .country = "PL",
      .city = "Białystok",
      .zipcode = "15-370",
      .street = "Wiejska",
      .number = "35C"};

  REQUIRE_THROWS_AS(ix1.insert(b_different_street), std::invalid_argument);

  // building belongs to a different city
  sentio::model::building b_different_city{
      .id = 1,
      .coords = sentio::spacial::coordinates(0, 0),
      .country = "PL",
      .city = "Suwałki",
      .zipcode = "15-370",
      .street = "Pogodna",
      .number = "35C"};

  REQUIRE_THROWS_AS(ix1.insert(b_different_city), std::invalid_argument);

  // building has no number
  sentio::model::building b_no_number{
      .id = 1,
      .coords = sentio::spacial::coordinates(0, 0),
      .country = "PL",
      .city = "Białystok",
      .zipcode = "15-370",
      .street = "Pogodna",
      .number = ""};

  REQUIRE_THROWS_AS(ix1.insert(b_no_number), std::invalid_argument);

  sentio::model::building duplicates[] = {
      sentio::model::building{
        .id = 1,
        .coords = sentio::spacial::coordinates(1, 2),
        .country = "PL",
        .city = "Białystok",
        .zipcode = "15-370",
        .street = "Pogodna",
        .number = "35C"},

      sentio::model::building{
        .id = 2,
        .coords = sentio::spacial::coordinates(3, 4),
        .country = "PL",
        .city = "Białystok",
        .zipcode = "15-371",
        .street = "Pogodna",
        .number = "35C"}};

  ix1.insert(duplicates[0]);

  REQUIRE(ix1.size() == 1);
  REQUIRE(ix1.begin()->first == "35C");
  REQUIRE(ix1.begin()->second.latitude() == 1);

  // TODO: see https://github.com/sentio-systems/kurier/issues/1
  // REQUIRE_THROW(ix1.insert(duplicates[1]), std::invalid_argument);
}