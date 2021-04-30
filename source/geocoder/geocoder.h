// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include "rpc/service.h"
#include "ner/geomodel.h"
#include "spacial/index.h"
#include "import/map_source.h"

// notes:
//  metrics to be collected:
//   - requests per second
//   - characters before full address resolved
//   - % of street name before full street resolved
//   - address resolved without selecting city
//   - time it takes to resolve prefix by character count

namespace sentio::geocoder
{

struct address_components
{
  std::optional<std::string> city;
  std::optional<std::string> street;
  std::optional<std::string> building;
  std::optional<std::string> zipcode;
};

/**
 * This is the type returned by all implementations of the geocoder backend as a
 * type that contains addressable exact matches and hints that are not addressable.
 */
struct lookup_result {
  std::vector<model::building> matches;
  std::vector<address_components> hints;
};

/**
 * This class implements the geocoding logic that translates textual addresses into
 * latlong coordinates. It also validates that a particular address is known to 
 * us and can be used in trip planning.
 * 
 * In general, the user is not allowed to use addresses that are not in our database.
 */
class geocoder
{
public:
  /**
   * All concrete geocoder backend implementations must adhere to this interface.
   * Currently two backends are implements: in-memory prefix tree, and sqlite fts.
   */
  class impl {
  public:
    /**
     * Called from multiple threads by all clients of the geocoder, and most 
     * commonly by the HTTP RPC service. This will forward calls to the concrete
     * implementation of the geocoding backend.
     * 
     * @param location used to narrow down the scope of the geocoder search space, this is
     *        the current user location in most cases. The backend implementation is supposed
     *        to serve results only from the same region the user is located in.
     * @param a parsed address, split into its base components.
     */
    virtual lookup_result lookup(
      spacial::region const& region,
      address_components components) const = 0;

    virtual ~impl() {}
  };

private:
  /**
   * Initialize an instance of the geocoder
   */
  geocoder(
    spacial::index const& locator,
    std::unique_ptr<impl> implementation);

private:
  /**
   * Given a free-form address string, this mehod will
   * invoke the geomodel neural network named entity 
   * recognition model to categorize its parts into address
   * components that can be used to query a concrete address
   * in one of the implementations.
   */
  address_components decompose(std::string text) const;

public:
  lookup_result lookup(
    spacial::coordinates const& location,
    std::string const& query_text,
    address_components overrides) const;

public:
  template <typename BackendType>
  static geocoder create(
    spacial::index const& locator,
    std::vector<import::region_paths> const& sources)
  {
    return geocoder(locator, 
      std::make_unique<BackendType>(sources));
  }

private:
  mutable geomodel geomodel_;
  std::unique_ptr<impl> impl_;
  spacial::index const& locator_;
};


}  // namespace sentio::services
