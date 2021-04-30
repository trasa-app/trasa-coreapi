// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <boost/property_tree/ptree.hpp>

/**
 * We're using Boost PropertyTree throughout the system as the 
 * default JSON representation for interpretting and manipulating
 * data exchange with external world actors.
 */
using json_t = boost::property_tree::ptree;