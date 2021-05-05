// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "utils/json.h"

namespace sentio::rpc
{
/**
 * 
 */
class auth 
{
public:
  auth(auth&&);

public:
  auth(json_t const& config);
  ~auth();
  
public:
  bool empty() const;
  size_t size() const;

public:
  std::optional<json_t> authorize(std::string_view const&) const;

private:
  class impl;
  std::unique_ptr<impl> impl_;
};

}