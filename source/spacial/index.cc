// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <iterator>
#include <execution>

#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "index.h"
#include "region.h"

namespace sentio::spacial
{

region::polygon to_polygon(std::istream& is, std::string name)
{
  std::string temp;
  std::string rname;
  int context_depth = 0;
  region::polygon output;

  // poly files define coordinates of the bounding polygon of a region.
  // The format is described here:
  // https://wiki.openstreetmap.org/wiki/Osmosis/Polygon_Filter_File_Format
  //
  // Poly files for a region contain actualy multiple polygons; the region
  // polygon and polygons that define neighbouring regions that fit within
  // the bounding rectange. We are interested only in the exact polygon for
  // the region identified by the variable @c name
  //
  // The stack is used to determine the current polygon being read
  // keep reading until the polygon name == name, then ingest its contents.
  //
  // example:
  //
  // województwo małopolskie
  // 1
  //  19.5293411 49.5730542
  //  19.5183851 49.5734240
  //  19.5028861 49.5817613
  //  19.4804077 49.5870350
  //  19.4718653 49.6005735
  //  19.4673762 49.6137628
  //  19.4731000 49.6184268
  //  ...
  //  19.9722965 50.5162508
  // END
  // END
  //
  // This is by no means a full (or even good) arser for poly files,
  // but it is good enough for our purposes and dataset.

  int line_counter = 0;
  while (std::getline(is, temp)) {
    ++line_counter;
    if (boost::trim_copy(temp).size() == 0) {
      continue;  // empty newline
    }
    if (!boost::starts_with(temp, " ")) {
      // this is either defining a begining of a polygon or an end
      if (boost::starts_with(temp, "END")) {
        if ((context_depth = std::max(0, context_depth - 1)) == 0) {
          rname = std::string();
        }
      } else {
        if (context_depth++ == 0) {
          rname = boost::trim_copy(temp);
        }
      }
    } else {
      if (context_depth == 2 && boost::iequals(rname, name)) {
        double lng = 0, lat = 0;
        std::stringstream ss(temp);
        ss >> lng;
        ss >> lat;
        output.outer().push_back(spacial::coordinates(lat, lng));
      }
    }
  }

  assert(!output.outer().empty());
  return output;
}

region::polygon to_polygon(std::string path, std::string name)
{
  std::ifstream polyfile(path, std::ios::in);
  return to_polygon(polyfile, name);
}

std::optional<region> index::locate(spacial::coordinates const& location) const
{
  std::vector<node_type> candidates;
  regions_.query(
    boost::geometry::index::contains(location),
    std::back_inserter(candidates));

  if (candidates.empty()) {
    return { };
  } else {
    for (auto c : candidates) {
      if (c.second.contains(location)) {
        return c.second;
      }
    }
    return {};
  }
}

void index::insert(region r)
{
  bounds_type bounding_box;
  boost::geometry::envelope(r.bounds(), bounding_box);
  if (std::any_of(regions_.begin(), regions_.end(), 
        [&r](auto const& region) { 
          return region.second.name() == r.name(); 
        })) {
    throw std::invalid_argument("duplicate region");
  }
  regions_.insert(std::make_pair(bounding_box, std::move(r)));
}

index::index(std::vector<import::region_paths> const& sources)
{
  for (auto const& source: sources) {
    insert(spacial::region(source.name, 
      to_polygon(source.poly, source.name)));
  }
}

}  // namespace sentio::index