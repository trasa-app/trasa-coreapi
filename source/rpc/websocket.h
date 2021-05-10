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
  
class websocket_session
  : public std::enable_shared_from_this<websocket_session>
{
public:
  using socket_type = boost::asio::ip::tcp::socket;
  
public:
  static constexpr size_t max_message_size = 4 * 1024 * 1024; // 4MB

public:
  /**
   * Create a new websocket session from a connected socket and take over ownership
   * of all resources associated with this connection.
   */
  websocket_session(
    socket_type&& socket,
    auth const& guard,
    service_map_t const& services,
    boost::asio::io_context& ioctx);
  ~websocket_session();

public:
  void start();

private:
  class impl;
  std::unique_ptr<impl> impl_;
};


class websocket_server
{
public:
  websocket_server(
    boost::asio::ip::tcp::endpoint ep,
    auth const& guard,
    service_map_t const& services,
    boost::asio::io_context& ioctx);
  ~websocket_server();

public:
  void start();

private:
  class impl;
  std::unique_ptr<impl> impl_;

};

}