// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "auth.h"
#include "utils/log.h"

#include <array>
#include <chrono>
#include <future>
#include <sstream>
#include <cassert>
#include <shared_mutex>
#include <unordered_map>

#include <curl/curl.h>
#include <jwt-cpp/jwt.h>

#include <boost/algorithm/string.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/algorithm/replace_if.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>

namespace sentio::rpc
{

std::string to_string(json_t const& o) {
  std::stringstream ss;
  boost::property_tree::write_json(ss, o);
  return ss.str();
}

class validator
{
public:
  static validator rs256(
    std::string kid,
    std::string name, 
    std::string iss, 
    std::string aud,
    std::string cert)
  {
    return validator(kid, name, iss, aud, 
      jwt::algorithm::rs256(cert));
  }

  static validator hs256(
    std::string kid,
    std::string name, 
    std::string iss, 
    std::string aud,
    std::string key)
  {
    return validator(kid, name, iss, aud, 
      jwt::algorithm::hs256(key));
  }

private:
  template <typename Algorithm>
  validator(
    std::string kid,
    std::string name, 
    std::string iss, 
    std::string aud,
    Algorithm algo) 
    : kid_(kid), iss_(iss), aud_(aud), name_(name)
    , verifier_(jwt::verify()
        .with_issuer(std::move(iss))
        .with_audience(std::move(aud))
        .allow_algorithm(algo))
  {
  }

public:
  std::string kid() const 
  { return kid_; }

  std::string name() const 
  { return name_; }

  std::string issuer() const
  { return iss_; }

  std::string audience() const
  { return aud_; }

public:
  void verify(jwt::decoded_jwt<jwt::picojson_traits> const& token) const 
  { verifier_.verify(token); }

private:
  std::string kid_, iss_, aud_, name_;
  jwt::verifier<jwt::default_clock, jwt::picojson_traits> verifier_;
};

std::stringstream download_string(std::string url) 
{ 
  std::shared_ptr<CURL> curl(
    curl_easy_init(), 
    curl_easy_cleanup);

  std::stringstream ss;
  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &ss);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, 
    +[](void *buffer, size_t, size_t nmemb, void *stream) {
      auto& s = *reinterpret_cast<std::stringstream*>(stream);
      s.write(static_cast<const char*>(buffer), nmemb);
      return nmemb;
    });
  
  if (auto res = curl_easy_perform(curl.get()); res != CURLE_OK) {
    errlog << "curl error while downloading auth keys: "
           << curl_easy_strerror(res);
    throw std::runtime_error(curl_easy_strerror(res));
  } else {
    return ss;
  }
}

std::vector<validator> read_validator(json_t const& entry) {
  auto construct = validator::rs256;

  if (entry.get<std::string>("type") == "jwt+rs256") {
    construct = validator::rs256;
  } else if (entry.get<std::string>("type") == "jwt+hs256") {
    construct = validator::hs256;
  } else {
    fatallog << "unsupported auth method: "
             << entry.get<std::string>("type");
    throw std::runtime_error("unsupported auth method");
  }

  std::vector<validator> output;
  if (entry.get_child("keys").size() == 0) {
    json_t keys;
    auto instream = download_string(entry.get<std::string>("keys"));
    boost::property_tree::read_json(instream, keys);
    for (auto const& key: keys) {
      output.push_back(construct(
        key.first, 
        entry.get<std::string>("name"),
        entry.get<std::string>("issuer"),
        entry.get<std::string>("audience"),
        key.second.get_value<std::string>()));
    }
  } else {
    for (auto const& key: entry.get_child("keys")) {
      output.push_back(construct(
        key.first, 
        entry.get<std::string>("name"),
        entry.get<std::string>("issuer"),
        entry.get<std::string>("audience"),
        key.second.get_value<std::string>()));
    }
  }
  return output;
}

class auth::impl 
{
public:
  impl(json_t const& cfg)
  : cfg_(cfg)
  {
    refresh_ = std::thread([this]() {
      while (true) {
        // Google refreshes their Firebase auth 
        // tokens every 3600 seconds. Redownload
        // remote keys and keep them in memory
        refresh_auth_keys(cfg_);
        std::this_thread::sleep_for(
          std::chrono::seconds(3600));
      }
    });
  }

public:
  json_t authorize(std::string_view const& token) const
  { 
    std::shared_lock lock(mutex_);
    auto decoded = jwt::decode(std::string(token));
    if (auto it = keys_.find(decoded.get_key_id()); it != keys_.end()) {
      it->second.verify(decoded);

      json_t output;
      output.add("upn", decoded.get_payload_claim("phone_number").as_string());
      output.add("idp", it->second.name());
      return output;
    } else {
      throw std::runtime_error("kid not trusted");
    }
  }

public:
  size_t size() const 
  { 
    std::shared_lock lock(mutex_);
    return keys_.size(); 
  }

  bool empty() const 
  { 
    std::shared_lock lock(mutex_);
    return keys_.empty(); 
  }

private:
  void refresh_auth_keys(json_t const& cfg)
  {
    std::unique_lock lock(mutex_);
    for (auto const& keyset: cfg) {
      try {
        for (auto const& v: read_validator(keyset.second)) {
          if (keys_.find(v.kid()) == keys_.end()) {
            keys_.emplace(v.kid(), v);
          }
        }
      } catch(const std::exception& e) {
        warnlog << "failed to import auth jwk: " << e.what() 
                << ". key def: " << to_string(keyset.second);
      }
    }

    assert(!keys_.empty());
    for (auto const& kv: keys_) {
      infolog << "activated authentication key " << kv.first
              << " with issuer " << kv.second.issuer() 
              << " with audience " << kv.second.audience();
    }
  }

private:
  json_t const& cfg_;
  std::thread refresh_;
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, validator> keys_;
};


auth::~auth() {}

auth::auth(auth&& other)
  : impl_(std::move(other.impl_)) {}

auth::auth(json_t const& cfg)
  : impl_(std::make_unique<impl>(cfg))
{
}

size_t auth::size() const 
{ return impl_->size(); }

bool auth::empty() const 
{ return impl_->empty(); }

std::optional<json_t> auth::authorize(std::string_view const& token) const
{ 
  try {
    return impl_->authorize(token);
  } catch (std::exception const& e) {
    warnlog << "auth failed for token " << token << " because " << e.what();
    return std::optional<json_t>();
  }
}


}