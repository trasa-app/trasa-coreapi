// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "sqlite_fts.h"
#include "utils/meta.h"

#include <boost/algorithm/string.hpp>

#define sqlite_ensure_ok(res, db)                         \
  do {                                                    \
    if (res != SQLITE_OK) {                               \
      std::cerr << "sqlite error: "                       \
                << (const char*)sqlite3_errmsg(db.get())  \
                << std::endl;                             \
      throw std::runtime_error(sqlite3_errmsg(db.get())); \
    }                                                     \
  } while (false)

namespace sentio::geocoder
{

static std::wstring_convert<std::codecvt_utf8<wchar_t>> g_conv;

static address_components sanitize(address_components components)
{
  static std::locale utf8("en_US.UTF-8");
  auto sanitize_component = [](auto& component) {
    if (component.has_value()) {
      std::wstring wstr = g_conv.from_bytes(component.value());
      std::replace_if(wstr.begin(), wstr.end(), 
        [](auto const& c) {
          return !(std::isalnum(c, utf8) || c == '/' || 
                  c == '-' || c == ' ' || c == '.');
        }, ' ');
      component = g_conv.to_bytes(wstr);
    }
  };
  
  sanitize_component(components.building);
  sanitize_component(components.city);
  sanitize_component(components.street);
  sanitize_component(components.zipcode);
  return components;
}

sqlite_backend::sqlite_backend(
  std::vector<import::region_paths> const& sources)
{
  for (auto const& source: sources) {
    std::clog << "indexing " << source.name << std::endl;
    sqlite3* handle = nullptr;
    auto openresult = sqlite3_open_v2(
      source.addressbook.c_str(), &handle, 
      SQLITE_OPEN_READONLY, nullptr);
    if (openresult != SQLITE_OK) {
      throw std::runtime_error("failed to open addressbook db");
    }

    regions_.emplace(source.name, handle);
  }
}

lookup_result sqlite_backend::building_matches(
  sqlite3_ptr const& regiondb,
  std::string const& streetname,
  std::string const& buildingno,
  std::optional<std::string> const& city,
  std::optional<std::string> const& zipcode) const
{
  std::stringstream sqlss;
  sqlss << "SELECT * FROM building ";
  sqlss << "WHERE building MATCH '{street alt_street}: \"" << streetname << "\"* ";
  sqlss << "AND {number}: \"" << buildingno << "\"* ";

  if (city.has_value()) {
    sqlss << "AND {city alt_city}: \"" << city.value() << "\"* ";
  }

  if (zipcode.has_value()) {
    sqlss << "AND {zipcode}: \"" << zipcode.value() << "\"* ";
  }

  sqlss << "' COLLATE NO_PL_ACCENTS ";
  sqlss << "ORDER BY city, number";

  sqlite3_stmt *stmt = nullptr;
  std::string query(sqlss.str());
  
  scope_exit cleanup([&stmt, &regiondb]() {
    if (stmt != nullptr) {
      sqlite_ensure_ok(sqlite3_finalize(stmt), regiondb);
    }
  });

  sqlite_ensure_ok(sqlite3_prepare_v2(
    regiondb.get(), query.c_str(), -1, &stmt, nullptr), 
    regiondb);

  lookup_result output;
  for(int step = sqlite3_step(stmt); 
           step == SQLITE_ROW; 
           step = sqlite3_step(stmt)) {
    auto building = model::building {
      .id = sqlite3_column_int64(stmt, 0),
      .coords = spacial::coordinates(
        sqlite3_column_double(stmt, 1),
        sqlite3_column_double(stmt, 2)),
      .country = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
      .city = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
      .zipcode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)),
      .street = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)),
      .number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))
    };
    boost::to_upper(building.number);
    output.matches.push_back(std::move(building));
  }

  return output;
}

lookup_result sqlite_backend::street_hints(
  sqlite3_ptr const& regiondb,
  std::string const& streetname,
  std::optional<std::string> city) const
{
  std::stringstream sqlss;
  sqlss << "SELECT DISTINCT city, street FROM building ";
  sqlss << "WHERE building MATCH '{street alt_street}: \"" << streetname << "\"* ";
  
  if (city.has_value()) {
    sqlss << "AND {city}: \"" << city.value() << "\"* ";
  }

  sqlss << "' COLLATE NO_PL_ACCENTS ";
  sqlss << "ORDER BY street, city";

  sqlite3_stmt *stmt = nullptr;
  std::string query(sqlss.str());
  
  scope_exit cleanup([&stmt, &regiondb]() {
    if (stmt != nullptr) {
      sqlite_ensure_ok(sqlite3_finalize(stmt), regiondb);
    }
  });

  sqlite_ensure_ok(sqlite3_prepare_v2(
    regiondb.get(), query.c_str(), -1, &stmt, nullptr), 
    regiondb);

  lookup_result output;
  for(int step = sqlite3_step(stmt); 
           step == SQLITE_ROW; 
           step = sqlite3_step(stmt)) {
    output.hints.push_back(address_components {
      .city = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
      .street = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
      .building = std::optional<std::string>(),
      .zipcode = std::optional<std::string>()
    });
  }

  return output;
}

lookup_result sqlite_backend::lookup(
  spacial::region const& region, 
  address_components components) const
{
  auto const& dbptr = regions_.at(region.name());

  // remove all potentially dangerous characters that
  // might expose the underlying sqlite to sql injection.
  components = sanitize(components);

  // cases:
  //  1. trying to locate the street
  //     1.a street present in multiple cities
  //  2. found street, trying to locate building
  //     2.b street and building present in multiple cities

  // having a building number, means that we might potentially have
  // an exact location, so return addressable matches. 
  // Otherwise the search space is too wide and only return hints.
  if (components.building.has_value()) {
    if (!components.street.has_value()) {
      return lookup_result(); // too wide, return nothing
    }

    return building_matches(dbptr,
      std::move(*components.street),
      std::move(*components.building),
      std::move(components.city),
      std::move(components.zipcode));
  } else {
    if (!components.street.has_value()) {
      return lookup_result(); // nothing, searching for cities makes no sense
    }

    // don't have a building number, but we can attempt
    // to find city-street pairs that match the input text
    return street_hints(dbptr,
      std::move(*components.street), 
      std::move(components.city));
  }
}

}
