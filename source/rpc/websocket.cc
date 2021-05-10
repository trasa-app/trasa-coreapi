// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "websocket.h"
#include "utils/log.h"

#include <list>
#include <thread>
#include <boost/beast/websocket.hpp>

namespace sentio::rpc
{

namespace net = boost::asio;
namespace web = boost::beast;
namespace ws = web::websocket;
using tcp = net::ip::tcp;

/**
 * Handles the state needed to maintain a connection with a connected client.
 * This data structure is maintained for each open connection, so be conservative
 * with the size and memory layout of this structure.
 */
class websocket_session::impl
{
private:  // types
  /**
   * This type of body is used only to signal HTTP error
   * using status codes only and no content.
   */
  using error_body_t = web::http::empty_body;

  /**
   * JSON-RPC is always in text format using POST method.
   */
  using string_body_t = web::http::string_body;

  using buffer_type = web::flat_buffer;
  using stream_type = web::websocket::stream<web::tcp_stream>;

public:
  impl(
    socket_type&& socket,
    auth const& guard,
    service_map_t const& services,
    boost::asio::io_context& ioctx)
  : ws_(std::move(socket))
  , guard_(guard)
  , services_(services)
  , ioctx_(ioctx)
{
  ws::permessage_deflate pmd;
  pmd.client_enable = true;
  pmd.server_enable = true;
  pmd.compLevel = 3;
  ws_.set_option(pmd);
  ws_.auto_fragment(false);
  ws_.read_message_max(websocket_session::max_message_size);
}

public:
  void start()
  {
    ws_.set_option(
    ws::stream_base::timeout::suggested(
      web::role_type::server));


    ws_.async_accept(boost::beast::bind_front_handler(
      [](auto) {
        //todo
      }));
  }

private:
  stream_type ws_;
  buffer_type buffer_;

private:
  auth const& guard_;
  service_map_t const& services_;
  boost::asio::io_context& ioctx_;
};


class websocket_server::impl
{
public:
  impl(
    tcp::endpoint ep,
    auth const& guard,
    service_map_t const& services,
    net::io_context& ioctx)
  : guard_(guard)
  , services_(services)
  , ioctx_(ioctx)
  , acceptor_(ioctx_)
{
  acceptor_.open(ep.protocol());
  acceptor_.set_option(net::socket_base::reuse_address(true));
  acceptor_.bind(ep);
  acceptor_.listen(net::socket_base::max_listen_connections);
}

public:
  void start()
  {
    acceptor_.async_accept(
    [this](web::error_code ec, tcp::socket socket) {
      if (ec) {
        errlog << "ip connection accept error: " << ec.message();
      } else {
        // transfer ownership of the accepted socket to a new 
        // session that will self destruct when its closed. Its
        // lifetime is managed by the enabled_shared_from_this mechanism.
        std::make_shared<websocket_session>(
          std::move(socket), guard_, services_, ioctx_)->start();
      }
      start();
    });
  }

private:
  auth const& guard_;
  service_map_t const& services_;
  boost::asio::io_context& ioctx_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

websocket_session::~websocket_session() {}
websocket_session::websocket_session(
  websocket_session::socket_type&& socket,
  auth const& guard,
  service_map_t const& services,
  boost::asio::io_context& ioctx)
  : impl_(std::make_unique<impl>(
      std::move(socket), 
      guard, services, ioctx))
{ }

void websocket_session::start()
{ impl_->start(); }

websocket_server::~websocket_server() {}
websocket_server::websocket_server(
  tcp::endpoint ep,
  auth const& guard,
  service_map_t const& services,
  net::io_context& ioctx)
  : impl_(std::make_unique<impl>(ep, guard, services, ioctx))
{ }

void websocket_server::start()
{ impl_->start(); }

}