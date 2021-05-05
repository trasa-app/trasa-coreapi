// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "auth.h"
#include <jwt-cpp/jwt.h>

namespace sentio::rpc
{

auth::auth(json_t const&)
{}

size_t auth::size() const 
{ return 0; }

bool auth::empty() const 
{ return size() == 0; }

std::optional<json_t> auth::authorize(std::string_view const&) const
{ return std::optional<json_t>(); }

}