// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <vector>

#include "rpc/service.h"
#include "model/address.h"
#include "geocoder/geocoder.h"

namespace sentio::services
{

/**
 * Interprets a user-generated geocoding query into type safe fields.
 */
class geocoder_request
{
public:
  /**
   * Specifies the way the address was captured by the user.
   */
  enum class capture_mode {
    /**
     * The user used a textbox to type in the paddress or parts of it.
     * This is the default mode and is used if no mode is specified.
     */
    text, 

    /**
     * The user scanned an address using a camera and the input is the
     * result of OCR-ing a photo.
     */
    camera
  };

public:
  geocoder_request(json_t json);

public:
  /**
   * This is the value of the free-text input text from the user.
   * 
   * The vast majority of requests are expected to have only this field populated
   * as the components of the address will be extracted through libparse, however in 
   * some edge cases, some requests will override some components of the text.
   */
  std::string text() const;

  /**
   * Specifies the way the address was entered on the client side.
   * This could be textbox-based input, an OCR-ed image, a link, etc.
   */
  capture_mode mode() const;

  /**
   * The current location of the user when this query was issued. It is used to 
   * narrow down the search space for potential addresses. Currently the system will
   * not match any addresses outside of user's province.
   * 
   * If this location was outside of supported regions, then the entire query will
   * fail with a 412 (precondition failed) error.
   */
  spacial::coordinates location() const;

/**
 * Components of the address
 * 
 * By default the request object will attempt to parse the text field from the
 * request and extract those components from the input text. Those fields here will
 * contain those components, however under some circumstances the request object can
 * override a particular component and it will take precedence over the result of
 * the parsed text.
 */
  geocoder::address_components overrides() const;

private:
  json_t json_;
};

/**
 * Represent the output of invoking the geocoder rpc service, consisting of
 * hints that get the user closer to the target address, or a set of exact 
 * matches that can be selected as the target address along with its coordinates.
 */
class geocoder_response
{
public:
  geocoder_response(
    std::vector<model::building> matches,
    std::vector<geocoder::address_components> hints);
  geocoder_response(geocoder::lookup_result result);

public:
  /**
   * This is a set of concrete addressable locations that have 
   * known coordinates and can be used as waypoints in trip planning.
   */
  std::vector<model::building> const& matches() const;

  /**
   * This is a set of non-addressable suggestions that can be used to 
   * fill parts of an address to get the user closer to a matched building.
   *
   * Examples of this might be street name, street+city, etc.
   */
  std::vector<geocoder::address_components> const& hints() const;

public:
  /**
   * Serializes the collection of matches and suggestions into an output
   * json object that is returned to the caller over the wire.
   */
  json_t to_json() const;

private:
  std::vector<model::building> matches_;
  std::vector<geocoder::address_components> hints_;
};

/**
 * This is the front-facing RPC service that invokes the geocoder
 * logic inside the geocoder/ code directory.
 */
class geocoder_service final 
  : public rpc::service_base
{
public:
  geocoder_service(
    spacial::index const& worldix, 
    std::vector<import::region_paths> const& sources,
    json_t const& configsection);

public:
  /**
   * Gets called from multiple thread by the rpc http server.
   * The params object is the JSON request content as it arrived
   * from the user.
   * 
   * This method will invoke logic implemented under /geocoder/ to
   * serve responses to end users.
   */
  json_t invoke(json_t params, rpc::context) const override;

private:
  geocoder::geocoder engine_;
};

}