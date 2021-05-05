// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <list>
#include <sstream>
#include <thread>
#include <iostream>

#include <boost/stacktrace.hpp>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include "server.h"
#include "service.h"
#include "error.h"
#include "utils/log.h"

namespace sentio::rpc
{
using namespace boost::asio;
using namespace boost::beast;
using namespace boost::beast::http;

namespace // detail
{
  template <typename BodyType>
  bool is_healthcheck(http::request<BodyType> const& req)
  {
    return req.method() == verb::get && 
           req.target() == "/healthcheck";
  }
}

/**
 * This class represents an asynchronous HTTP accept->read->process->respond
 * runloop. It is meant to be an isolated agent that has several instances
 * running concurrenlty (depending on the number of CPU cores), serving
 * external clients over HTTP.
 */
class http_worker : private boost::noncopyable
{
private:  // types
  /**
   * This type of body is used only to signal HTTP error
   * using status codes only and no content.
   */
  using error_body_t = boost::beast::http::empty_body;

  /**
   * JSON-RPC is always in text format using POST method.
   */
  using string_body_t = boost::beast::http::string_body;

public:  // construction
  http_worker(ip::tcp::acceptor& acceptor, service_map_t const& svcs,
              auth const& guard)
    : guard_(guard) 
    , svcs_(svcs)
    , acceptor_(acceptor)
    , socket_(acceptor.get_executor())
  { assert(svcs_.size() != 0); }

public:
  void start() { do_accept(); }

private:
  void clear_state()
  {
    error_code ec;
    response_ = {};
    eresponse_ = {};
    buffer_.clear();    // reset consume/commit ptrs
    socket_.close(ec);  // close sockets from any previous connections
  }

  /**
   * Begins asynchronously waiting for new connections from remote
   * hosts. This is the default state of the worker when it is not
   * processing any requests.
   */
  void do_accept()
  {
    // getting here, means that this worker is not in
    // the middle of receiving a request, processing or
    // sending a response. Clear all buffers that might
    // have leftover from previous requests.
    clear_state();
    acceptor_.async_accept(socket_, [this](error_code error) {
      if (error) {
        do_accept();
      }  // if failed, discard and start over
      else {
        do_read_request();
      }  // otherwise, process current message
    });
  }

  /**
   * Begins asynchronosly receiving HTTP requests and
   * saves them into a fixed 4kb buffer.
   */
  void do_read_request()
  {
    http::async_read(
        socket_, buffer_, request_,
        [this](auto error, size_t len) { do_handle_request(error, len); });
  }

  /**
   * Invokes the process request method that actually
   * does all the per-request heavy lifting, and wraps
   * it with some error handling and resume logic.
   */
  void do_handle_request(boost::beast::error_code error, size_t)
  {
    if (error) {
      return do_accept();
    }

    // in case of a failure, discard and
    // continue accepting new connections.
    try {
      process_request(std::move(request_));
    } catch (not_authorized const& e) {
      return terminate_with_error(e, status::unauthorized,
                                  [this]() { do_accept(); });
    } catch (bad_request const& e) {
      return terminate_with_error(e, status::bad_request,
                                  [this]() { do_accept(); });
    } catch (std::invalid_argument const& e) {
      return terminate_with_error(e, status::bad_request,
                                  [this]() { do_accept(); });
    } catch (not_implemented const& e) {
      return terminate_with_error(e, status::not_implemented,
                                  [this]() { do_accept(); });
    } catch (server_error const& e) {
      return terminate_with_error(e, status::internal_server_error,
                                  [this]() { do_accept(); });
    } catch (bad_method const& e) {
      return terminate_with_error(e, status::method_not_allowed,
                                  [this]() { do_accept(); });
    } catch (std::exception const& e) {
      return terminate_with_error(e, status::internal_server_error,
                                  [this]() { do_accept(); });
    } catch (...) {
      return terminate_with_error(std::runtime_error("unknown error"),
                                  status::internal_server_error,
                                  [this]() { do_accept(); });
    }
  }

  context get_context_from_token(
      boost::asio::ip::tcp::socket const& socket,
      http::header<true, http::fields> const& headers)
  {
    if (headers.count(http::field::authorization) != 1) {
      throw not_authorized();
    }
    auto authval = headers.at(http::field::authorization);
    static const std::string_view prefix("bearer ");
    if (!boost::istarts_with(authval, prefix)) {
      throw bad_request();
    }
    std::string_view tokenview(authval.data(), authval.size());
    tokenview.remove_prefix(prefix.size());

    if (auto decoded = guard_.authorize(tokenview); decoded.has_value()) {
      return context {
        .uid = decoded->get<std::string>("upn"),
        .role = decoded->get<std::string>("role"),
        .remote_ep = socket.remote_endpoint()
      };
    } else {
      throw not_authorized();
    }
  }

  /**
   * This gets called once the server finished receiving the request content
   * and the entire request is stored in the buffer. This is where processing
   * happens.
   */
  void process_request(http::request<string_body_t> req)
  {
    // This server handles only JSON-RPC requests which are JSON objects
    // sent through POST to one of the exposed endpoints, all other verbs
    // are not supported.
    if (req.method() != verb::post) {
      if (is_healthcheck(req)) {
        confirm_healthcheck();
        return;
      } else {
        // respond with HTTP error 405 and terminate connection.
        errlog << "unsupported request method: " 
                  << req.method();
        throw bad_method();
      }
    }
    
    std::string method;
    boost::property_tree::ptree params;

    // attempt to parse JSON request and extract required fields
    // we're going to fail the request if its not a valid JSON
    // or one of the required fields is missing.
    try {
      boost::property_tree::ptree reqjson;
      std::stringstream ss(req.body());
      boost::property_tree::read_json(ss, reqjson);
      params = reqjson.get_child("params");
      method = reqjson.get<std::string>("method");
    } catch (...) {
      errlog << "bad request body: " << req.body();
      throw bad_request();
    }

    // getting here means that we have a valid JSON-RPC object,
    // route to the right handler, or fail if not implemented.
    auto svcit = svcs_.find(method);
    if (svcit == svcs_.end()) {  // make sure that the method is impented
      errlog << "unknown rpc method: " << method;
      throw bad_method(method.c_str());
    }

    context request_context;

    // if the service requires authenticated token
    if (svcit->second->authenticated()) {
      // ensure user has a valid jwt token in case the
      // service required authenticated requests.
      // exrtract the userid from the payload
      request_context = get_context_from_token(socket_, req.base());
    } else {
      // no creds or role, just the remote ep
      request_context.remote_ep = socket_.remote_endpoint();
    }
    

    // getting here means that we have a valid JSON-RPC request and
    // that there is a registered handler for the requested method.
    // invoke the method and store the resulting JSON object,
    // then serialize to string.
    std::stringstream outss;

    // call handler logic and store response
    auto resjson = svcit->second->invoke(
      std::move(params), 
      std::move(request_context));

    boost::property_tree::write_json(outss, resjson);

    // we have a valid response
    response_.keep_alive(false);
    response_.set(field::content_type, "application/json; charset=utf-8");
    response_.body() = outss.str();
    response_.prepare_payload();
    http::async_write(socket_, response_, [this](error_code ec, size_t) {
      socket_.shutdown(ip::tcp::socket::shutdown_send, ec);
      do_accept();
    });
  }

  template <typename Then, typename Exception>
  void terminate_with_error(Exception const& e, http::status status,
                            Then&& cont)
  {
    auto const& ep = socket_.remote_endpoint();
    errlog << "request error [" << ep << "]: " << e.what();
    errlog << boost::current_exception_diagnostic_information();
    errlog << boost::stacktrace::stacktrace();
    eresponse_.result(status);     // HTTP error code (=/= 200)
    eresponse_.keep_alive(false);  // disconnect
    eresponse_.prepare_payload();  // serialize
    http::async_write(
        socket_, eresponse_,
        [this, cont = std::forward<Then>(cont)](error_code error, size_t) {
          socket_.shutdown(ip::tcp::socket::shutdown_send, error);
          cont();
        });
  }

  // TODO: consider refactoring out into separate thread/component
  void confirm_healthcheck()
  {
    auto const& ep = socket_.remote_endpoint();
    infolog << "health check from " << ep << ": ok";
    auto checkresponse = std::make_shared<response<empty_body>>();
    checkresponse->result(http::status::ok);
    checkresponse->keep_alive(false);
    http::async_write(socket_, *checkresponse, 
      [this, checkresponse](error_code error, size_t) {
        socket_.shutdown(ip::tcp::socket::shutdown_both, error);
        do_accept();
      });
  }

private:  // internal state
  auth const& guard_;
  service_map_t const& svcs_;
  ip::tcp::acceptor& acceptor_;
  ip::tcp::socket socket_;
  flat_static_buffer<4096> buffer_;
  request<string_body_t> request_;
  response<string_body_t> response_;
  response<error_body_t> eresponse_;
};

// clang-format off
void run_server(config config, service_map_t services)
{
  assert(config.listen_port != 0);
  assert(!config.guard.empty());
  assert(!config.listen_ip.empty());

  // start two http workers per one CPU core in parallel
  int workers_count = std::thread::hardware_concurrency() * 2;
  infolog << "starting JSON-RPC server on " 
       << config.listen_ip << ":"
       << config.listen_port
       << " using " << workers_count 
       << " worker threads.";

  std::list<http_worker> workers;
  std::list<std::thread> threads;

  io_context ioctx{workers_count};
  ip::tcp::acceptor acceptor(ioctx,
    ip::tcp::endpoint(
      ip::make_address(config.listen_ip), 
      config.listen_port));

  // initialize workers
  for (auto i = 0; i < workers_count; ++i) {
    workers.emplace_back(acceptor, services, config.guard);
    workers.back().start();
    threads.emplace_back([&ioctx]() { ioctx.run(); });
  }
  
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

}  // namespace sentio::kurier