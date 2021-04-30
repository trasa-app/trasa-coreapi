// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>
#include <random>
#include <sstream>

#include <boost/algorithm/string.hpp>

#include "dataset.h"
#include "transform.h"

namespace sentio::geomodel
{

std::random_device g_random_device;

struct labeled_string
{
  sample::chars_type chars;
  sample::labels_type labels;
};

void append_component(
  labeled_string& dst,
  std::string const& component, 
  geocoder::address_label label)
{
  using geomodel = sentio::geocoder::geomodel_impl;

  if (component.empty()) {
    return; // not all components of an address are always present
  }

  for (wchar_t const& c: geomodel::encode_string(component)) {
    dst.chars.push_back(c);
    dst.labels.push_back(label);
  }
}

std::vector<sample> make_plain(
  model::building const& b, torch::DeviceType d)
{
  labeled_string full; //, without_zip, without_city, without_building;
  std::mt19937 generator(g_random_device());
  std::uniform_int_distribution<> randval(0, 1000);

  // full
  append_component(full, b.street,  geocoder::address_label::street);
  append_component(full, " ",       geocoder::address_label::other);
  append_component(full, b.number,  geocoder::address_label::building);
  
  // sometimes the separator is just a space, 
  // sometimes a comma with a space
  if (!b.city.empty()) {
    if (randval(generator) > 500) {
      append_component(full, " ",       geocoder::address_label::other);
    } else {
      append_component(full, ", ",       geocoder::address_label::other);
    }
    append_component(full, b.city,    geocoder::address_label::city);
  }

  if (!b.zipcode.empty()) {
    if (randval(generator) > 500) {
      append_component(full, " ",       geocoder::address_label::other);
    } else {
      append_component(full, ", ",       geocoder::address_label::other);
    }
    append_component(full, b.zipcode, geocoder::address_label::zipcode);
  }

  return { 
    sample(std::move(full.chars), std::move(full.labels), d, b),
  };
}

std::vector<sample> make_abbreviated(
  model::building const&, torch::DeviceType)
{
  return { };
}

}
