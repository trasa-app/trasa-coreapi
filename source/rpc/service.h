// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <optional>
#include <boost/asio/ip/tcp.hpp>
#include <boost/property_tree/ptree.hpp>

#include "utils/json.h"

namespace sentio::rpc
{

struct context 
{
  std::string uid; // user id
  std::string idp; // identity provider
  boost::asio::ip::tcp::endpoint remote_ep;
};


/**
 * Base class for all JSON-RPC HTTP request handler.
 */
class service_base
{
public:
  /**
   * Returns true if the handler requires requests to have a valid
   * authorization header.
   */
  virtual bool authenticated() const { return true; }

  /**
   * Implements the main per-perquest service logic.
   */
  virtual json_t invoke(json_t params, context ctx) const = 0;

  /**
   * For derived classes destruction.
   */
  virtual ~service_base() {}

public: // noncopyable
  service_base(service_base const&) = delete;
  service_base& operator=(service_base const&) = delete;

public: // movable
  service_base(service_base&&) = default;
  service_base& operator=(service_base&&) = default;

public: // default trivial constructor
  service_base() = default;
};

/**
 * A shorthand for creating an instance of a JSON-RPC method handler
 * and wrapping it in an unique_ptr.
 */
template <typename ServiceT>
std::unique_ptr<service_base> create_service(ServiceT&& instance)
{
  return std::make_unique<ServiceT>(std::forward<ServiceT>(instance));
}

}