// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>

#include "utils/json.h"

namespace sentio::rpc
{
class invalid_token 
  : public std::runtime_error 
{
public:
  invalid_token();
};

class jwt
{
public:
  static json_t read_payload(
    std::string_view const& token, 
    std::string_view const& secret);

  static std::string issue_token(
    std::string_view const& data,
    std::string_view const& secret);
};

}
