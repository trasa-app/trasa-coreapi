// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>
#include <unordered_map>

#include <sqlite3.h>

#include "rpc/service.h"
#include "spacial/index.h"
#include "geocoder/geocoder.h"

namespace sentio::geocoder
{
class sqlite_backend final
  : public geocoder::impl {

private:
  struct sqlite3_destructor {
    void operator()(sqlite3* instance) const {
      if (instance != nullptr) {
        std::cerr << "closing sqlite instance." << std::endl;
        auto result = sqlite3_close(instance);
        if (result != SQLITE_OK) {
          throw std::runtime_error("failed closing sqlite3");
        }
      }
    }
  };

  using sqlite3_ptr = std::unique_ptr<
    sqlite3, sqlite3_destructor>;

public:
  sqlite_backend(std::vector<import::region_paths> const& sources);

public:
  lookup_result lookup(
    spacial::region const& region,
    address_components components) const override;

private:
  /**
   * Used when the user is still trying to find the street name,
   * and nothing else is known. This is usually the first stage of 
   * providing hints to the user (unless the address is fully pasted
   * at once).
   */
  lookup_result street_hints(
    sqlite3_ptr const& regiondb,
    std::string const& streetname,
    std::optional<std::string> city = {}) const;

  lookup_result building_matches(
    sqlite3_ptr const& regiondb,
    std::string const& streetname,
    std::string const& buildingno,
    std::optional<std::string> const& city = {},
    std::optional<std::string> const& zipcode = {}) const;

private:
  std::unordered_map<std::string, sqlite3_ptr> regions_;
};
}