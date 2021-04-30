// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <vector>
#include <torch/torch.h>
#include <model/address.h>
#include <geocoder/ner/geomodel.h>

namespace sentio::geomodel
{

class sample;

// Transforms are things that take a building and return a randomised
// set of sample objects that represent variations of that building with
// some extra fields, some fields removed or morphed

std::vector<sample> make_plain(model::building const& b, torch::DeviceType d);

/**
 * For multiword street names this will generate versions of the
 * street name with its abbreviated version
 */
std::vector<sample> make_abbreviated(model::building const& b, torch::DeviceType d);

/**
 * For a given address this generates its variations with a random suite added
 * after the building number in one of the following formats:
 * 
 *  <street name> <building number> [m <number> | mieszkanie <number> | 
 *                                   lokal <number> | lok. <number> | lok <number | 
 *                                   etc]
 */
std::vector<sample> add_random_suite(model::building const& b, torch::DeviceType d);

}