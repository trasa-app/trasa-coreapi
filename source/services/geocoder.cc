// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "geocoder.h"
#include "geocoder/geocoder.h"
#include "geocoder/sqlite_fts/sqlite_fts.h"

#include <boost/algorithm/string.hpp>

namespace sentio::services
{

inline static void throw_if_missing(json_t const& json, const char* path)
{
  if (!json.get_child_optional(path).has_value()) {
    std::string message = "missing request param ";
    throw std::invalid_argument(message + path);
  }
}

geocoder_request::geocoder_request(json_t json)
  : json_(std::move(json)) 
{
  throw_if_missing(json_, "text");
  throw_if_missing(json_, "location.latitude");
  throw_if_missing(json_, "location.longitude");
}

std::string geocoder_request::text() const 
{ return json_.get<std::string>("text"); }

geocoder_request::capture_mode geocoder_request::mode() const
{
  if (auto m = json_.get_optional<std::string>("mode"); m.has_value()) {
    if (boost::iequals(m.value(), "camera")) {
      return capture_mode::camera;
    }
    if (boost::iequals(m.value(), "text")) {
      return capture_mode::text;
    }
  }
  return capture_mode::text;
}

spacial::coordinates geocoder_request::location() const 
{
  return spacial::coordinates(
    json_.get<double>("location.latitude"),
    json_.get<double>("location.longitude"));
}

geocoder::address_components geocoder_request::overrides() const
{
  geocoder::address_components output;

  auto set_if_present = [this](const char* path) {
    if (auto val = json_.get_optional<std::string>(path); val.has_value()) {
      return std::optional<std::string>(val.value());
    } else {
      return std::optional<std::string>();
    }
  };

  output.city = set_if_present("components.city");
  output.street = set_if_present("components.street");
  output.building = set_if_present("components.building");
  output.zipcode = set_if_present("components.zipcode");

  return output;
}

geocoder_response::geocoder_response(
  std::vector<model::building> matches,
  std::vector<geocoder::address_components> hints)
  : matches_(std::move(matches))
  , hints_(std::move(hints))
{
}

geocoder_response::geocoder_response(
  geocoder::lookup_result result)
  : matches_(std::move(result.matches))
  , hints_(std::move(result.hints))
{
}

std::vector<model::building> const& 
geocoder_response::matches() const
{ return matches_; }

std::vector<geocoder::address_components> const& 
geocoder_response::hints() const
{ return hints_; }

json_t geocoder_response::to_json() const
{
  json_t output, hints, matches;
  for (auto const& match: matches_) {
    matches.push_back(std::make_pair("", match.to_json()));
  }

  for (auto const& hint: hints_) {
    json_t hintobj;
    if (hint.city.has_value()) {
      hintobj.add("city", hint.city.value());
    }
    if (hint.street.has_value()) {
      hintobj.add("street", hint.street.value());
    }
    if (hint.building.has_value()) {
      hintobj.add("building", hint.building.value());
    }
    if (hint.zipcode.has_value()) {
      hintobj.add("zipcode", hint.zipcode.value());
    }
    if (hintobj.size() != 0) {
      hints.push_back(std::make_pair("", std::move(hintobj)));
    }
  }

  if (matches.size() != 0) {
    output.add_child("matches", std::move(matches));
  }

  if (hints.size() != 0) {
    output.add_child("hints", std::move(hints));
  }

  return output;
}

static geocoder::geocoder create_backend(
  std::string const& mode,
  spacial::index const& worldix, 
  std::vector<import::region_paths> const& sources)
{
  if (boost::iequals("sqlite_fts", mode)) {
    return sentio::geocoder::geocoder::create<geocoder::sqlite_backend>(worldix, sources);
  } else {
    throw std::invalid_argument("unrecognized geocoder mode");
  }
}

geocoder_service::geocoder_service(
  spacial::index const& worldix, 
  std::vector<import::region_paths> const& sources,
  json_t const& configsection)
  : engine_(create_backend(
      configsection.get<std::string>("mode"), worldix, sources))
{
}

json_t geocoder_service::invoke(json_t params, rpc::context) const
{
  geocoder_request request(std::move(params));
  geocoder_response response(engine_.lookup(
    request.location(), request.text(), request.overrides()));
  return response.to_json();
}

}