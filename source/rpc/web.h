// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include "server.h"

#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace sentio::rpc
{

class web_server
{
public:
  web_server(
    config const& config,
    service_map_t const& services);
  ~web_server();

public:
  void start();

private:
  class impl;
  std::unique_ptr<impl> impl_;
};

}