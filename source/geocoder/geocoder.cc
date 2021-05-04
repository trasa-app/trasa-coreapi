// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>
#include <chrono>
#include <optional>
#include <iostream>
#include <sqlite3.h>

#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "rpc/error.h"
#include "utils/log.h"
#include "utils/meta.h"
#include "model/address.h"

#include "geocoder.h"
#include "ner/geomodel.h"
#include "sqlite_fts/sqlite_fts.h"

namespace sentio::geocoder
{

static std::wstring_convert<std::codecvt_utf8<wchar_t>> g_conv;

/**
 * This function applies a set of heuristics that adjust the reognized address 
 * components to match the usage scenarios of the geocoder on the mobile 
 * application when the user used a textbox to input the address.
 */
static address_components practical_text_adjust(address_components components)
{ 
  // 1. libpostal sometimes thinks that a street is a city name, when nothing
  //    else than a single text phrase is entered. So in this case if city has
  //    a value and the street is empty, we swap them. Because finding out
  //    the street name has higher precedence
  
  bool only_city_captured = 
    !components.building.has_value() &&
    !components.zipcode.has_value() &&
    (components.city.has_value() && !components.street.has_value());

  if (only_city_captured) {
    std::swap(components.city, components.street);
    return components;
  }

  return components;
} 

geocoder::geocoder(
    spacial::index const& locator,
    std::unique_ptr<impl> implementation)
  : geomodel_(load_embedded_model())
  , impl_(std::move(implementation))
  , locator_(locator)
{
}

address_components geocoder::decompose(std::string text) const
{
  // invoke NER neural network and return labels tensor
  auto tensor = geomodel_(text);
  
  // convert torch tensor to STL vector of labels
  std::vector<address_label> labels(static_cast<size_t>(tensor.size(0)));
  for (int64_t i = 0; i < tensor.size(0); ++i) {
    labels[i] = address_label(static_cast<int32_t>(
      tensor[i].item<int64_t>()));
  }

  // returns a substring of the address string between the first and
  // the last occurance of a recognized character label. This algorithm
  // is chosen because sometimes spaces and other separator characters 
  // wihin a single address component are classified as "other".
  auto extract_component = [&labels, &text](address_label label) {
    auto first_occurance = std::find_if(labels.begin(), labels.end(), 
      [label](auto const& char_label) { return char_label == label; });
    if (first_occurance != labels.end()) {
      auto last_occurance = std::find_if(labels.rbegin(), labels.rend(), 
        [label](auto const& char_label) { return char_label == label; }).base() + 1;
      auto start_index = std::distance(labels.begin(), first_occurance);
      auto token_length = std::distance(first_occurance, last_occurance);
      return std::optional<std::string>(text.substr(start_index, token_length));
    }

    return std::optional<std::string>();
  };

  address_components output {
    .city = extract_component(address_label::city),
    .street = extract_component(address_label::street),
    .building = extract_component(address_label::building),
    .zipcode = extract_component(address_label::zipcode)
  };
  return output;
}

lookup_result geocoder::lookup(
  spacial::coordinates const& location,
  std::string const& query_text,
  address_components overrides) const
{
  auto region = locator_.locate(location);
  if (!region.has_value()) {
    throw std::invalid_argument("location not supported");
  } else {
    
    // split one address string into itso components using the
    // geomodel neural network NLP component.
    address_components components = decompose(query_text);

    auto override_if_empty = [](auto& component, auto const& override_v) {
      if (override_v.has_value()) {
        component = override_v;
      }
    };

    override_if_empty(components.city, overrides.city);
    override_if_empty(components.street, overrides.street);
    override_if_empty(components.building, overrides.building);
    override_if_empty(components.zipcode, overrides.zipcode);

    return impl_->lookup(*region, 
      practical_text_adjust(
        std::move(components)));
  }
}

}
