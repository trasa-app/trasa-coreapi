// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>
#include <unordered_map>

#include "rpc/service.h"
#include "spacial/index.h"
#include "import/map_source.h"

#include "geocoder/geocoder.h"
#include "geocoder/prefix_tree/region.h"

namespace sentio::geocoder
{

class prefix_tree_backend final
  : public geocoder::impl
{
public:
  prefix_tree_backend(std::vector<import::region_paths> const& sources);

public:
  lookup_result lookup(
    spacial::region const& region,
    address_components components) const override;

// private:
//   services::geocoder_response suggest_all_exact_captures(
//     services::geocoder_request const& query, 
//     index::region const& index) const;

//   services::geocoder_response suggest_no_captures(
//     std::string const& query, 
//     index::region const& index) const;

private:
  std::unordered_map<std::string, region> regions_;
};

}
