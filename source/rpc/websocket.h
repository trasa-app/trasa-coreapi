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
  using buffer_type = boost::beast::flat_buffer;
  using socket_type = boost::asio::ip::tcp::socket;
  using stream_type = boost::beast::websocket::stream<boost::beast::tcp_stream>;

public:
  const size_t max_message_size = 4 * 1024 * 1024; // 4MB

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

public:
  void start();

private:
  stream_type ws_;
  buffer_type buffer_;

private:
  auth const& guard_;
  service_map_t const& services_;
  boost::asio::io_context& ioctx_;
};


class websocket_server
{
public:
  websocket_server(
    boost::asio::ip::tcp::endpoint ep,
    auth const& guard,
    service_map_t const& services,
    boost::asio::io_context& ioctx);

public:
  void start();

private:
  auth const& guard_;
  service_map_t const& services_;
  boost::asio::io_context& ioctx_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

}