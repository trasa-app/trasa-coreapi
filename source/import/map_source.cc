// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>
#include <regex>
#include <memory>
#include <iostream>
#include <filesystem>
#include <iterator>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>

#include <openssl/md5.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "map_source.h"
#include "utils/future.h"

namespace sentio::import
{
/**
 * This type imlements a limited URI parser.
 *
 * It is by no means a complete URI parser implementation,
 * rather its implements the minimum subset of the parsing
 * functionality that is needed to parse map-data url links.
 */
class uri
{
public:
  enum class protocol { http, https, s3 };

public:
  /**
   * Constructs an instance or the URI type.
   *
   * This constructor will attempt too parse the string
   * parameter passed to it, and throws if the value is
   * not a valid uri.
   */
  uri(std::string value)
      : full_(std::move(value))
  {
    static const std::regex urlregex(
        "^(.*)://([A-Za-z0-9\\-\\.]+)(:[0-9]+)?(.*)$",
        std::regex::ECMAScript | std::regex::icase);

    std::smatch matches;
    std::regex_search(full_, matches, urlregex);

    if (matches.size() != 5) {
      throw std::invalid_argument("invalid uri");
    }

    if (matches[1] == "http") {
      protocol_ = protocol::http;
    } else if (matches[1] == "https") {
      protocol_ = protocol::https;
    } else if (matches[1] == "s3") {
      protocol_ = protocol::s3;
    } else {
      throw std::invalid_argument("protocol not supported");
    }

    host_ = std::move(matches[2]);
    path_ = std::move(matches[4]);

    if (matches[3].length() == 0) {
      if (protocol_ == protocol::http) {
        port_ = 80;
      } else if (protocol_ == protocol::https) {
        port_ = 443;
      } else if (protocol_ == protocol::s3) {
        port_ = 0;
      } else {
        port_ = boost::lexical_cast<uint16_t>(matches[3]);
      }
    }
  }

public:
  uint16_t port() const { return port_; }
  protocol proto() const { return protocol_; }
  std::string const& host() const { return host_; }
  std::string const& path() const { return path_; }
  std::string filename() const
  {
    std::filesystem::path p(path());
    return p.filename().string();
  }

public:
  std::string const& str() const { return full_; }
  operator std::string() const { return str(); }

private:
  uint16_t port_;
  std::string host_;
  std::string path_;
  std::string full_;
  protocol protocol_;
};

Aws::S3::Model::GetObjectRequest make_s3_content_request(std::string s3url)
{
  uri s3path(std::move(s3url));
  assert(s3path.proto() == uri::protocol::s3);
  Aws::S3::Model::GetObjectRequest request;
  request.SetBucket(s3path.host());
  request.SetKey(s3path.path());
  return request;
}

Aws::S3::Model::HeadObjectRequest make_s3_metadata_request(std::string s3url)
{
  uri s3path(std::move(s3url));
  assert(s3path.proto() == uri::protocol::s3);
  Aws::S3::Model::HeadObjectRequest request;
  request.SetBucket(s3path.host());
  request.SetKey(s3path.path());
  return request;
}

std::filesystem::path local_object_path(Aws::S3::Model::GetObjectRequest const& req)
{
  // ask the operating system to provide a temp directory.
  // we don't really care about the actual storage path, as long
  // as the data file is accessible on the filesystem from this
  // process. One less value to configure.
  auto tempdir = std::filesystem::temp_directory_path();
  auto fname = std::filesystem::path(req.GetKey()).filename();
  auto destpath = tempdir / fname;
  return destpath;
}

/**
 * Checks if the remote s3 object is already downloaded locally from previous runs, and
 * if the md5 hash of the local file equals to the etag of the remote. If yes, then the
 * cached version is used instead and downloads is skipped. Note that the etag checksum 
 * algorithm for large files uploaded through multipart upload works on chunks, not on the
 * entire file. For more info search for "AWS S3 etag checksum multipart".
 */
std::optional<std::string> local_object_digest(Aws::S3::Model::GetObjectRequest const& req)
{
  auto localpath = local_object_path(req);
  if (std::filesystem::exists(localpath)) {
    const size_t chunk_size = 8 * 1024 * 1024;
    std::unique_ptr<char[]> buffer(new char[chunk_size]);
    std::ifstream readf(localpath, std::ios::binary);
    std::vector<std::array<unsigned char, MD5_DIGEST_LENGTH>> chunks_hashes;

    // for each chunk calculate a separate md5 checksum
    while (readf.good()) {
      MD5_CTX md5ctx;
      MD5_Init(&md5ctx);
      std::array<unsigned char, MD5_DIGEST_LENGTH> digest;
      readf.read(buffer.get(), chunk_size);
      MD5_Update(&md5ctx, buffer.get(), readf.gcount());
      MD5_Final(digest.data(), &md5ctx);
      chunks_hashes.push_back(digest);
    }

    // md5 byte to lowercase hex string
    auto pretty_digest = [](auto digest) {
      std::string output;
      const auto char_digest = reinterpret_cast<const char*>(&digest);
      const auto char_digest_size = sizeof(digest);
      boost::algorithm::hex_lower(
        char_digest, 
        char_digest + char_digest_size,
        std::back_inserter(output));
      return output;
    };

    // compound / large file etag checksum
    if (chunks_hashes.size() > 1) {
      MD5_CTX md5ctx;
      MD5_Init(&md5ctx);
      for (auto const& chunk: chunks_hashes) {
        MD5_Update(&md5ctx, chunk.data(), chunk.size());
      }

      std::array<unsigned char, MD5_DIGEST_LENGTH> digest;
      MD5_Final(digest.data(), &md5ctx);
      return (pretty_digest(digest) + "-" + std::to_string(chunks_hashes.size()));
    } else {
      // small file checksum
      return pretty_digest(chunks_hashes.front());
    }
  }
  return {};
}

std::future<std::string> save_s3_object_async(Aws::S3::S3Client const& s3client,
                                              std::string const& s3uri)
{
  return std::async([&s3client, &s3uri]() {
    auto request = make_s3_content_request(s3uri);
    
    // retreive remote object etag only without content
    auto metadata = s3client.HeadObject(
      make_s3_metadata_request(s3uri));

    // calculate md5 checksum of the local file
    auto destpath = local_object_path(request);
    auto localdigest = local_object_digest(request);

    // remove quotes from AWS response string
    auto remotedigest = std::string_view(
      &metadata.GetResult().GetETag()[1], 
      metadata.GetResult().GetETag().size() - 2);

    if (localdigest.has_value()) {
      if (localdigest.value() == remotedigest) {
        std::clog << "using cached version of " << s3uri << " @ " << destpath 
                  << " [" << localdigest.value() << "]" << std::endl;
        return destpath.string();
      }
    }

    // we're setting the stream factory to a file stream because otherwise
    // the contents of the S3 Object will be stored in memory. This will
    // kill the process with many parallel downloads of larde map data files.
    // Redirect all incoming bytes straight to the destination file.
    request.SetResponseStreamFactory([destpath]() {
      return Aws::New<Aws::FStream>(
          "turbo_s3fileAlloc", destpath.string(),
          std::ios_base::out | std::ios::binary | std::ios::trunc);
    });

    // begin download to file stream
    auto result = s3client.GetObject(request);

    if (!result.IsSuccess()) {
      std::cerr << "s3 get failed for " << s3uri << ": " << result.GetError()
                << std::endl;

      // this will eventually lead to the process being terminated
      // and this is ok, because this code runs on process start and
      // any failure here means that down the line we will be working
      // with incomplete map data, so we want to die early enough and
      // diagnose those failures.
      throw std::runtime_error(result.GetError().GetMessage());
    }

    // Getting here means that GetObject was executed successfully.
    // return path to the file that received the S3 object bytes.
    return destpath.string();
  });
}

std::vector<region_paths> region_paths::from_config(
  json_t const& systemconfig) 
{
  const auto addressbook_path = "addressbook." + 
    systemconfig.get<std::string>("geocoder.mode");

  const auto osrm_path = "osrm." + 
    systemconfig.get<std::string>("routing.algorithm");

  std::vector<region_paths> output;
  for (auto const& region: systemconfig.get_child("regions")) {
    if (region.second.get_optional<bool>("enabled").value_or(true)) {
      output.emplace_back(region_paths {
        .name = region.second.get<std::string>("name"),
        .addressbook = region.second.get<std::string>(addressbook_path),
        .poly = region.second.get<std::string>("poly"),
        .osrm = region.second.get<std::string>(osrm_path)
      });
    }
  }
  return output;
}

std::future<region_paths> region_paths::download_async(
  region_paths const& rp)
{
  static std::once_flag once;
  static Aws::S3::S3Client s3client;

  std::call_once(once, []() {
    Aws::Client::ClientConfiguration cfg;
    cfg.region = Aws::Region::EU_CENTRAL_1;
    s3client = Aws::S3::S3Client(cfg);
  });

  std::clog << "starting region data download for " << rp.name << std::endl;
  return std::async([rp = std::move(rp)]() {
    auto ab_path = save_s3_object_async(s3client, rp.addressbook);
    auto poly_path = save_s3_object_async(s3client, rp.poly);
    auto osrm_path = save_s3_object_async(s3client, rp.osrm);
    sentio::utils::wait_for_all(ab_path, poly_path, osrm_path);
    std::clog << "region " << rp.name << " data download completed." 
              << std::endl;

    try {
      return region_paths {
        .name = std::move(rp.name),
        .addressbook = ab_path.get(),
        .poly = poly_path.get(),
        .osrm = osrm_path.get()
      };
    } catch (std::exception const& e) {
      std::cerr << "downloading region data failed for " 
                << rp.name << ": " << e.what() << std::endl;
      throw; 
    }
  });
}

}  // namespace sentio::import