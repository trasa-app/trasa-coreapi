// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <stdexcept>

namespace sentio::rpc
{
/**
 * Thrown when a request does not have a valid authentication token.
 * Gets translated to HTTP 401 error.
 */
class not_authorized : public std::runtime_error
{
public:
  not_authorized();
  not_authorized(const char* msg);
};

/**
 * Thrown when the JSON-RPC request could not be
 * parsed or has some missing or invalid fields.
 */
class bad_request : public std::runtime_error
{
public:
  bad_request();
  bad_request(const char* msg);
};

/**
 * Thrown when the JSON-RPC request could not be
 * parsed or has some missing or invalid fields.
 */
class server_error : public std::runtime_error
{
public:
  server_error();
  server_error(const char* msg);
};

/**
 * Thrown when a request attempts to invoke a non-existing JSON-RPC method.
 * Gets translated to HTTP 501 Not implemented error code.
 */
class bad_method : public std::runtime_error
{
public:
  bad_method();
  bad_method(const char* msg);
};

/**
 * Thrown when a request attempts to invoke a non-existing JSON-RPC method.
 * Gets translated to HTTP 501 Not implemented error code.
 */
class not_implemented : public std::runtime_error
{
public:
  not_implemented();
  not_implemented(const char* msg);
};

}  // namespace sentio::kurier