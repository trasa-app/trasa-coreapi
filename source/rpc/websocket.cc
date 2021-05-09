// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "websocket.h"
#include "utils/log.h"
#include <boost/beast/websocket.hpp>

namespace sentio::rpc
{

namespace net = boost::asio;
namespace web = boost::beast;
namespace ws = web::websocket;
using tcp = net::ip::tcp;

websocket_session::websocket_session(
  websocket_session::socket_type&& socket,
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
  ws_.read_message_max(max_message_size);
}

void websocket_session::start()
{
  ws_.set_option(
    ws::stream_base::timeout::suggested(
      web::role_type::server));


  ws_.async_accept(boost::beast::bind_front_handler(
    [](auto) {
      //todo
    }));
}

websocket_server::websocket_server(
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

void websocket_server::start()
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

}