// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <array>
#include <ctime>
#include <vector>
#include <sstream>
#include <iterator>

#include <openssl/sha.h>
#include <openssl/hmac.h>

#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/algorithm/replace_if.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>

#include "jwt.h"

namespace sentio::rpc
{
namespace base64
{
  std::string decode(std::string_view const& val)
  {
    using namespace boost::archive::iterators;
    using iterator = std::string_view::const_iterator;
    using It = transform_width<binary_from_base64<iterator>, 8, 6>;
    return boost::algorithm::trim_right_copy_if(
        std::string(It(std::begin(val)), It(val.end())),
        [](char c) { return c == '\0'; });
  }

  template <typename Range>
  std::string decode(Range const& r)
  {
    return decode(std::string_view(r.begin(), r.size()));
  }

  template <typename It>
  std::string encode(It beg, It end)
  {
    using namespace boost::archive::iterators;
    using b64it = base64_from_binary<transform_width<It, 6, 8>>;
    auto bit = b64it(beg), eit = b64it(end);
    std::string encoded(bit, eit);
    boost::replace_if(encoded, boost::is_any_of("+/="), '-');
    return encoded;  // urlencoded
  }

  template <typename Range>
  std::string encode(Range const& r)
  {
    return encode(std::string_view(r.begin(), r.size()));
  }
}  // namespace base64

namespace hash
{
  /**
   * Computes HMAC-SHA256 hash of the range of bytes between dbegin and dend.
   * The key is specified using the range of bytes between kbegin and kend.
   */
  template <typename ItK, typename ItD>
  std::vector<uint8_t> hmac_sha256(ItD dbegin, ItD dend, ItK kbegin, ItK kend)
  {
    std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH, uint8_t(0));
    auto hash_length = static_cast<uint32_t>(digest.size());
    auto data_pointer = reinterpret_cast<const uint8_t*>(&(*dbegin));
    auto data_length = std::distance(dbegin, dend);

    auto key_pointer = std::addressof(*kbegin);
    auto key_length = std::distance(kbegin, kend);

    HMAC_CTX* context = HMAC_CTX_new();
    HMAC_Init_ex(context, key_pointer, key_length, EVP_sha256(), nullptr);
    HMAC_Update(context, data_pointer, data_length);
    HMAC_Final(context, digest.data(), &hash_length);
    HMAC_CTX_free(context);

    return digest;
  }

  template <typename RangeD, typename RangeK>
  std::vector<uint8_t> hmac_sha256(RangeD const& data, RangeK const& key)
  {
    return hmac_sha256(std::begin(data), std::end(data), std::begin(key),
                       std::end(key));
  }
}


invalid_token::invalid_token() 
  : std::runtime_error("invalid jwt token") 
{
}

inline void auth_ensure(bool condition)
{ if (!condition) { throw invalid_token(); } }


json_t jwt::read_payload(
  std::string_view const& token, 
  std::string_view const& secret)
{
  using namespace boost::algorithm;
  using namespace boost::posix_time;

  using iterator = std::string_view::const_iterator;
  std::vector<boost::iterator_range<iterator>> chunks;
  split(chunks, token, boost::is_any_of("."));
  auth_ensure(chunks.size() == 3);

  // verify signature before parsing payload
  std::string user_signature = 
    std::string(chunks[2].begin(), 
                chunks[2].end());
  boost::replace_all(user_signature, "_", "-");
  size_t contentlen = std::distance(chunks[0].begin(), chunks[1].end());
  std::string_view content(chunks[0].begin(), contentlen);
  auto hash = hash::hmac_sha256(content, secret);
  auto encoded_hash = base64::encode(hash.begin(), hash.end());
  auth_ensure(equals(user_signature, encoded_hash));

  // decode header and payload
  auto header = base64::decode(chunks[0]);
  auto payload = base64::decode(chunks[1]);

  try {
    // here we can fail for three reasons:
    // - the payload is not a valid json
    // - the timestamp is not in a valid format (e.g. month > 12)
    // - the token has expired and "exp" is less than now().
    // catch iso time string parser errors and translate them to 401
    // make sure token is not expired
    std::stringstream ss(std::move(payload));
    boost::property_tree::ptree payload_json;
    boost::property_tree::read_json(ss, payload_json);

    auto exp = payload_json.get<time_t>("exp");
    auth_ensure(exp > std::time(nullptr));

    auto nbf = payload_json.get<time_t>("nbf");
    auth_ensure(nbf < std::time(nullptr));

    // extract unique user id
    return payload_json;
  } catch (std::exception const& e) {
    throw invalid_token();
  } 
}

std::string jwt::issue_token(
  std::string_view const& data, 
  std::string_view const& secret)
{
  // base64({ "alg": "HS256", "typ": "JWT" })
  static const char* b64header = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";

  std::stringstream builder;
  builder << b64header << "." << base64::encode(data.begin(), data.end());
  auto hash = hash::hmac_sha256(builder.str(), secret);
  std::string sig = base64::encode(hash.begin(), hash.end());
  builder << "." << sig;
  return builder.str();
}

}
