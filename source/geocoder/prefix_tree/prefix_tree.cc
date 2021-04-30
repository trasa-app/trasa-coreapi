// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <mutex>
#include <execution>

#include "rpc/error.h"
#include "prefix_tree.h"
#include "import/region_reader.h"

namespace sentio::geocoder
{
  prefix_tree_backend::prefix_tree_backend(
    std::vector<import::region_paths> const& sources)
  {
    throw std::logic_error(
      "prefix_tree implementation "
      "is disabled now. use sqlite_fts instead");
      
    std::mutex ixlock;

    std::for_each(std::execution::par_unseq,
      std::begin(sources), std::end(sources),
      [this, &ixlock](auto const& source) {
        std::clog << "indexing " << source.name << std::endl;
        region regionix(source.name);
        
        auto bend = sentio::import::building_record_iterator();
        auto bit = sentio::import::building_record_iterator(source.addressbook);

        for (; bit != bend; ++bit) {
          regionix.insert(*bit);
        }
        regionix.seal();
        std::clog << "indexing " << source.name << " done ("
                  << regionix.size() << " buildings)." << std::endl;

        {  // mutually exclusive inserts into worldix
          std::scoped_lock<std::mutex> lk(ixlock);
          regions_.emplace(source.name, std::move(regionix));
        }
      });
  }

  lookup_result prefix_tree_backend::lookup(
    spacial::region const& region,
    address_components components) const
  {
    boost::ignore_unused(region);
    boost::ignore_unused(components);
    throw std::logic_error("not implemented now");
    return lookup_result();
  }
}

/**
 *  General high-level anatomy of building coordinates lookup:
 *
 *    world worldix;
 *    coordinates user_location;
 *    auto region = worldix.find(user_location);
 *     if (region != worldix.end()) {
 *       	auto street = region->find("Wiejska");
 *    	if (street != region->end()) {
 *    		auto city = street->find("Białystok");
 *    		if (city != street->end()) {
 *    			auto building = city->find("35C");
 *    			if (building != city->end()) {
 *    				return building->coords;
 *    			}
 *    		}
 *     }
 *   }
 */

// namespace sentio::index
// {

// geocoder_service_prefix_tree::geocoder_service_prefix_tree(
//   spacial::index const& locator, 
//   std::vector<sentio::import::region_paths> const& sources)
//     : locator_(locator)
// {
  // std::mutex ixlock;
  // std::for_each(std::execution::par_unseq,
  // std::begin(sources), std::end(sources),
  // [this, &ixlock](auto const& source) {
  //   std::clog << "indexing " << source.name << std::endl;
  //   index::region regionix(source.name);
    
  //   for (auto& building : source.addressbook()) {
  //     regionix.insert(std::move(building));
  //   }
  //   regionix.seal();
  //   std::clog << "indexing " << source.name() << " done ("
  //             << regionix.size() << " buildings)." << std::endl;

  //   {  // mutually exclusive inserts into worldix
  //     std::scoped_lock<std::mutex> lk(ixlock);
  //     regions_.emplace(source.name(), std::move(regionix));
  //   }
  // });
// }

// services::geocoder_response
// geocoder_service_prefix_tree::suggest(
//   services::geocoder_request const& query) const
// {
//   auto region = locator_.locate(query.coords());
//   if (!region.has_value()) {
//     throw rpc::not_authorized("location not supported");
//   }

//   // get the region-specifix sub prefix-tree
//   auto regionit = regions_.find(region->name());

//   // 1. case with no captures, only query text (default)
//   if (!query.city() && !query.city() && !query.building()) {
//     // if no captures, then it must have something in the text
//     if (query.text().empty()) {
//       throw std::invalid_argument("empty queries not allowed");
//     }
//     return suggest_no_captures(query.text(), regionit->second);
//   }

//   // 2. case with all captures, exact match, 
//   //    except building number prefix match
//   if (query.city() && query.street()) {
//     return suggest_all_exact_captures(query, regionit->second);
//   }

//   return services::geocoder_response();
// }

// services::geocoder_response 
// geocoder_service_prefix_tree::suggest_all_exact_captures(
//   services::geocoder_request const& query, index::region const& index) const
// {
//   services::geocoder_response response;
//   auto street_match = index.find(query.street().value());
//   if (street_match == index.end()) {
//     return response;
//   }

//   auto city_match = street_match->second.find(query.city().value());
//   if (city_match == street_match->second.end()) {
//     return response;
//   }

//   auto brange = city_match->second
//     .prefix_match(query.text());

//   for (auto it = brange.first; it != brange.second; ++it) {
//     response.matches.push_back(
//       model::building {
//         .id = it->second.id,
//         .coords = it->second.coords,
//         .country = std::string(),
//         .city = city_match->first,
//         .zipcode = it->second.zipcode,
//         .street = street_match->first,
//         .number = it->first
//       });
//   }

//   return response;
// }

// services::geocoder_response geocoder_service_prefix_tree::suggest_no_captures(
//   std::string const& query, index::region const& index) const
// {
//   services::geocoder_response response;
//   auto exact_match = index.find(query);
//   if (exact_match != index.end()) {
//     // there is a known street name with this name,
//     // return all known street-city pairs as hints.
//     for (auto const& c: exact_match->second) {
//       response.hints.push_back(services::hint { 
//         .street = exact_match->first, 
//         .city = c.first,
//         .building = std::nullopt,
//         .zipcode = std::nullopt
//       });
//     }
//   } else {
//     auto srange = index.prefix_match(query);
//     auto hints_count = std::distance(srange.first, srange.second);
//     for (auto streetit = srange.first; streetit != srange.second; ++streetit) {
      
//       // if we have few matching street names, then hints should
//       // already include the cities they belong to.
//       if (hints_count <= 3) {
//         if (hints_count == 1 || streetit->second.size() <= 3) {
//           for (auto const& c: streetit->second) {
//             response.hints.push_back(services::hint { 
//               .street = streetit->first, 
//               .city = c.first,
//               .building = std::nullopt,
//               .zipcode = std::nullopt
//             });
//           }
//         } else {
//           response.hints.push_back(services::hint { 
//             .street = streetit->first,
//             .city = std::nullopt,
//             .building = std::nullopt,
//             .zipcode = std::nullopt
//           });
//         }
//       } else {
//         // too many matching streets to include cities,
//         // just help the user find the street name first
//         response.hints.push_back(services::hint { 
//           .street = streetit->first,
//           .city = std::nullopt,
//           .building = std::nullopt,
//           .zipcode = std::nullopt
//         });
//       }
//     }
//   }
//   return response;
// }

// }