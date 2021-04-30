// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <unordered_map>

#include "service.h"

namespace sentio::rpc
{

/**
 * Represents a mapping from method-name to service handler instance.
 * 
 * Whenever an HTTP request arrives on the wire, its body is first parsed as
 * JSON and then the "method" field is extracted. Individual methods correspond
 * to instances of classes that handle tham in this map.
 */
using service_map_t = 
  std::unordered_map<
    std::string,      // key
    std::unique_ptr<  // instance
      service_base
    >
  >;

/**
 * This type holds all the settings captured from the environment,
 * about the server configuration. Those settings are most often
 * captured from the command line parameters.
 */
struct config {
  /**
   * The IP on which the server will listen for requests.
   * Its recommended to use 0.0.0.0 (its also the default value if not
   * specified).
   */
  std::string listen_ip;

  /**
   * The port on which this server will be running.
   * The default value is 5000.
   */
  uint16_t listen_port;

  /**
   * The secret key used to sign and verify JWT tokens.
   */
  std::string secret;
};

/**
 * Starts HTTP server and serves JSON-RPC methods to external consumers.
 *
 * @param ip The IP address this server will listen on. Its recommended to use
 * 0.0.0.0
 * @param port The IP port this server will listen on.
 * @param services a map of string -> handler function that implement individual
 * RPC methods.
 */
void run_server(config config, service_map_t services);

}  // namespace sentio::kurier