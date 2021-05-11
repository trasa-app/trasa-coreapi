// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "web.h"
#include "error.h"
#include "utils/log.h"
#include "utils/meta.h"

#include <list>
#include <thread>

#include <boost/beast/websocket.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace sentio::rpc
{

namespace net = boost::asio;
namespace web = boost::beast;
namespace ws = web::websocket;
using tcp = net::ip::tcp;


namespace // detail
{
  template <typename BodyType>
  bool is_healthcheck(web::http::request<BodyType> const& req)
  {
    return req.method() == web::http::verb::get && 
           req.target() == "/healthcheck";
  }

  template <typename BodyType>
  bool is_cors_options(web::http::request<BodyType> const& req)
  {
    return req.method() == web::http::verb::options && 
           req.target() == "/";
  }
}


/**
 * Handles the state needed to maintain a connection with a connected client.
 * This data structure is maintained for each open connection, so be conservative
 * with the size and memory layout of this structure.
 */
class web_session
  : public std::enable_shared_from_this<web_session>
{
public:
  static constexpr size_t buffer_size = 64 * 1024; // 64KB

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

  using buffer_type = web::flat_static_buffer<buffer_size> ;
  using stream_type = web::websocket::stream<web::tcp_stream>;
  using socket_type = tcp::socket;
  using request_type = web::http::request<string_body_t>;
  using response_type = web::http::response<string_body_t>;
  using error_response_type = web::http::response<error_body_t>;

public:
  /**
   * Create a new websocket session from a connected socket and take over ownership
   * of all resources associated with this connection.
   */
  web_session(
    socket_type&& socket,
    auth const& guard,
    service_map_t const& services)
  : socket_(std::move(socket))
  , guard_(guard)
  , services_(services)
  , ioctx_(*socket_.get_executor().target<net::io_context>())
{
  tracelog << "web session started";
}

~web_session()
{ tracelog << "web session destructed"; }

public:
  void start()
  { http_start(); }

  void http_start(web::error_code = web::error_code(), size_t = 0)
  {
    request_ = request_type(); 
    response_ = response_type();
    web::http::async_read(socket_, ibuffer_, request_, 
      web::bind_front_handler(
        &web_session::on_http_read, shared_from_this()));
  }

private:
  void accept_ws_session(context const& context)
  {
    wsctx_ = context;
    ws_->auto_fragment(false);
    ws_->read_message_max(buffer_size);
    ws_->set_option(ws::permessage_deflate {
      .server_enable = true,
      .client_enable = true,
      .compLevel = 3
    });
    ws_->set_option(
      ws::stream_base::timeout::suggested(
        web::role_type::server));
    
    ws_->async_accept(
      request_,
      web::bind_front_handler( 
        &web_session::on_ws_accept, 
        shared_from_this()));
  }

  json_t invoke_rpc_method(json_t request, context const& ctx)
  {
    std::string method;
    std::optional<std::string> msgid;
    boost::property_tree::ptree params;

    // attempt to parse JSON request and extract required fields
    // we're going to fail the request if its not a valid JSON
    // or one of the required fields is missing.
    try {
      params = request.get_child("params");
      msgid = to_std(request.get_optional<std::string>("id"));
      method = request.get<std::string>("method");
    } catch (...) {
      throw bad_request();
    }

    // getting here means that we have a valid JSON-RPC object,
    // route to the right handler, or fail if not implemented.
    auto svcit = services_.find(method);
    if (svcit == services_.end()) {  // make sure that the method is impented
      errlog << "unknown rpc method: " << method;
      throw bad_method(method.c_str());
    }

    return svcit->second->invoke(std::move(params), ctx);
  }

  void process_ws_request() 
  { 
    json_t output;
    try {
      json_t parsed_request;
      std::stringstream ss;
      ss.write((char*)ibuffer_.data().data(), ibuffer_.data().size());
      tracelog << "ws request: " << ss.str();
      boost::property_tree::read_json(ss, parsed_request);
      output = invoke_rpc_method(parsed_request, *wsctx_);
    } catch (std::exception const& e) {
      errlog << "ws process error: " << e.what();
      output.add("jsonrpc", "2.0");
      output.add("error.message", "unspecified error");
    }

    std::stringstream ssout;
    boost::property_tree::write_json(ssout, output);
    std::string serialized = ssout.str();
    tracelog << "ws response: " << serialized;
    
    auto write = obuffer_.prepare(serialized.size());
    std::copy(serialized.begin(), serialized.end(), 
      reinterpret_cast<char*>(write.data()));
    obuffer_.commit(serialized.size());
  }

  void process_http_request() 
  { 
    // handle non-standard lightweight request types
    if (is_healthcheck(request_)) {
      confirm_healthcheck();
      return;
    } else if (is_cors_options(request_)) { 
      cors_headers_response();
      return;
    }

    json_t parsed_request;
    std::stringstream ss(request_.body());

    // authentication & authorization
    context request_context(
      get_context_from_token(
        socket_, request_.base()));

    // This server handles only JSON-RPC requests which are JSON objects
    // sent through POST to one of the exposed endpoints, all other verbs
    // are not supported.
    if (request_.method() == web::http::verb::post) {
      boost::property_tree::read_json(ss, parsed_request);
      json_t rpcresult = invoke_rpc_method(parsed_request, request_context);

      std::stringstream outss;
      boost::property_tree::write_json(outss, rpcresult);
      
      response_.body() = outss.str();
      apply_cors_headers(response_);
      response_.keep_alive(false);
      response_.set(
        web::http::field::content_type, 
        "application/json; charset=utf-8");
      response_.prepare_payload();

      web::http::async_write(socket_, response_, 
        web::bind_front_handler(
          &web_session::http_close,
          shared_from_this()));

    } else if (web::websocket::is_upgrade(request_)) {
      // alternatively clients can establish a websocket connection
      // and send the same json-rpc calls without reestablishing
      // connections.
      ws_.emplace(stream_type(std::move(socket_)));
      accept_ws_session(request_context);
    } else {
      errlog << "unsupported request method: " 
              << request_.method();
      throw bad_method();
    }
  };

  context get_context_from_token(
      boost::asio::ip::tcp::socket const& socket,
      web::http::header<true, web::http::fields> const& headers)
  {
    if (headers.count(web::http::field::authorization) != 1) {
      throw not_authorized();
    }
    auto authval = headers.at(web::http::field::authorization);
    static const std::string_view prefix("bearer ");
    if (!boost::istarts_with(authval, prefix)) {
      throw bad_request();
    }
    std::string_view tokenview(authval.data(), authval.size());
    tokenview.remove_prefix(prefix.size());

    if (auto decoded = guard_.authorize(tokenview); decoded.has_value()) {
      return context {
        .uid = decoded->get<std::string>("upn"),
        .idp = decoded->get<std::string>("idp"),
        .remote_ep = socket.remote_endpoint()
      };
    } else {
      throw not_authorized();
    }
  }

  template <typename Response>
  void apply_cors_headers(Response& res)
  {
    using field = web::http::field;
    res.set(field::access_control_allow_origin, "*");
    res.set(field::access_control_allow_headers, "authorization, content-type");
    res.set(field::access_control_allow_methods, "post");
    res.set(field::access_control_allow_credentials, "true");
  }

  void confirm_healthcheck()
  {
    auto const& ep = socket_.remote_endpoint();
    dbglog << "health check from " << ep << ": ok";
    eresponse_.result(web::http::status::ok);
    eresponse_.keep_alive(false);
    web::http::async_write(socket_, eresponse_, 
      web::bind_front_handler(
        &web_session::http_close, 
        shared_from_this()));
  }

  void cors_headers_response()
  {
    eresponse_.result(web::http::status::ok);
    eresponse_.keep_alive(false);
    apply_cors_headers(eresponse_);

    web::http::async_write(socket_, eresponse_, 
      web::bind_front_handler(
        &web_session::http_start, 
        shared_from_this()));
  }

private:
  void ws_async_read() {
    ibuffer_.clear();
    ws_->async_read(ibuffer_, web::bind_front_handler(
      &web_session::on_ws_read, shared_from_this()));
  }

  void on_http_read(web::error_code ec, size_t)
  {
    if (ec) {
      if (ec != web::http::error::end_of_stream) {
        errlog << "http socket error: " << ec.message();
      }
      socket_.close();
      return;
    }

    try {
      process_http_request();
    } catch (not_authorized const& e) {
      return terminate_with_error(e, web::http::status::unauthorized);
    } catch (bad_request const& e) {
      return terminate_with_error(e, web::http::status::bad_request);
    } catch (std::invalid_argument const& e) {
      return terminate_with_error(e, web::http::status::bad_request);
    } catch (not_implemented const& e) {
      return terminate_with_error(e, web::http::status::not_implemented);
    } catch (server_error const& e) {
      return terminate_with_error(e, web::http::status::internal_server_error);
    } catch (bad_method const& e) {
      return terminate_with_error(e, web::http::status::method_not_allowed);
    } catch (std::exception const& e) {
      return terminate_with_error(e, web::http::status::internal_server_error);
    } catch (...) {
      return terminate_with_error(
        std::runtime_error("unknown error"),
        web::http::status::internal_server_error);
    }
  }

  void on_ws_read(web::error_code ec, size_t)
  {
    if (ec) {
      if (ec != net::error::eof && 
          ec != net::error::connection_reset) {
        errlog << "ws error: " << ec.message();  
      }
      socket_.close();
      return;
    } else {
      process_ws_request();
      ws_->text(ws_->got_text());
      ws_->async_write(obuffer_.data(), web::bind_front_handler(
        &web_session::on_ws_write, shared_from_this()));
    }
  }

  void on_ws_write(web::error_code ec, size_t n)
  {
    if (ec) {
      errlog << "ws error: " << ec.message();
    }

    //obuffer_.consume(n);
    obuffer_.clear(); 
    ibuffer_.clear();

    if (ws_->is_open()) { 
      ws_async_read(); 
    }
  }

  void on_ws_accept(web::error_code ec)
  {
    if (ec) {
      errlog << "ws accept error: " << ec.message();
    } else {
      dbglog << "ws accepted, reading bytes";
      ws_async_read();
    }
  }
  
  void http_close(web::error_code, size_t)
  { socket_.close(); }


  template <typename Exception>
  void terminate_with_error(
    Exception const& e, 
    web::http::status status)
  {
    auto const& ep = socket_.remote_endpoint();
    errlog << "request error [" << ep << "]: " << e.what();
    errlog << boost::current_exception_diagnostic_information();
    eresponse_.result(status);     // HTTP error code (=/= 200)
    eresponse_.keep_alive(false);  // disconnect
    apply_cors_headers(eresponse_);
    eresponse_.prepare_payload();  // serialize

    web::http::async_write(socket_, eresponse_,
      web::bind_front_handler(
        &web_session::http_start, 
        shared_from_this()));
  }

  void on_ws_close(web::error_code ec)
  { infolog << "connection closed: " << ec.message(); }

private:
  socket_type socket_;
  std::optional<stream_type> ws_;
  buffer_type ibuffer_;
  buffer_type obuffer_;
  request_type request_;
  response_type response_;
  error_response_type eresponse_;
  std::optional<context> wsctx_;

private:
  auth const& guard_;
  service_map_t const& services_;
  net::io_context& ioctx_;
};


class web_server::impl
{
public:
  impl(
    tcp::endpoint ep,
    auth const& guard,
    service_map_t const& services)
  : guard_(guard)
  , services_(services)
  , acceptor_(ioctx_)
{
  acceptor_.open(ep.protocol());
  acceptor_.set_option(net::socket_base::reuse_address(true));
  acceptor_.bind(ep);
  acceptor_.listen(net::socket_base::max_listen_connections);
}

public:
  void start() {
    
    std::list<std::thread> instances;
    size_t concurrency = std::thread::hardware_concurrency() * 2;
    
    for (size_t i = 0; i < concurrency; ++i) {
      instances.emplace_back(std::thread([this](){
        BOOST_LOG_SCOPED_THREAD_TAG("tid", 
          sentio::logging::assign_thread_id());
        accept_next();
        ioctx_.run();
      }));
    }

    for (auto& th: instances) {
      if (th.joinable()) {
        th.join();
      }
    }
  }

private:
  void accept_next()
  {
    acceptor_.async_accept(
    [this](web::error_code ec, tcp::socket socket) {
      if (ec) {
        errlog << "ip connection accept error: " << ec.message();
      } else {
        // transfer ownership of the accepted socket to a new 
        // session that will self destruct when its closed. Its
        // lifetime is managed by the enabled_shared_from_this mechanism.
        std::make_shared<web_session>(
          std::move(socket), guard_, services_)->start();
      }
      accept_next();
    });
  }

private:
  auth const& guard_;
  service_map_t const& services_;
  boost::asio::io_context ioctx_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

tcp::endpoint from_config(config const& config) {
  return tcp::endpoint(
    net::ip::address::from_string(config.listen_ip),
    config.listen_port);
}

web_server::~web_server() {}
web_server::web_server(
  config const& config,
  service_map_t const& services)
  : impl_(std::make_unique<impl>(
      from_config(config), config.guard, services))
{ }

void web_server::start()
{ impl_->start(); }

}