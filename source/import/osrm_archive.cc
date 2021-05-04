// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <chrono>
#include <string>
#include <iostream>
#include <execution>
#include <filesystem>
#include <algorithm>
#include <iomanip>

#include <zlib.h>
#include <archive.h>
#include <archive_entry.h>

#include "utils/log.h"
#include "import/map_source.h"

namespace sentio::import 
{

class tarbz2_archive
{
private:
  static const size_t block_size = 10240;
  
  static inline int ensure_ok(int retval)
  {
    if (retval == ARCHIVE_FAILED || retval == ARCHIVE_FATAL) {
      throw std::runtime_error("libarchive error: " + std::to_string(retval));
    }
    return retval;
  }

public:
  tarbz2_archive(std::string const& filename) 
    : path_(std::move(filename))
    , a_(archive_read_new())
    , ext_(archive_write_disk_new())
  { 
    int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_FFLAGS;
    ensure_ok(archive_read_support_format_tar(a_.get()));
    ensure_ok(archive_read_support_filter_bzip2(a_.get()));
    ensure_ok(archive_write_disk_set_options(ext_.get(), flags));
    ensure_ok(archive_write_disk_set_standard_lookup(ext_.get()));
    ensure_ok(archive_read_open_filename(a_.get(), path_.c_str(), block_size));
  }

public:
  tarbz2_archive const& extract() const
  {
    archive_entry *entry = nullptr;
    while (true) {
      int retval = ensure_ok(archive_read_next_header(a_.get(), &entry));
      if (retval == ARCHIVE_EOF) { break; }

      std::filesystem::path entrypath(archive_entry_pathname(entry));
      entrypath = path_.parent_path() / entrypath;

      // check if already extracted by previous runs:
      if (std::filesystem::exists(entrypath)) {
        // get modification times:      
        auto arch_mtime = archive_entry_mtime(entry);
        auto local_mtime = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::file_clock::to_sys(
            std::filesystem::last_write_time(entrypath))
              .time_since_epoch()).count();
        
        // get sizes:
        auto arch_size = static_cast<size_t>(archive_entry_size(entry));
        auto local_size = std::filesystem::file_size(entrypath);

        // if already extracted, skip:
        if (arch_mtime == local_mtime && arch_size == local_size) {
          return (*this);
        }
      }

      archive_entry_set_pathname(entry, entrypath.c_str());
      archive_entry_set_perm(entry, 0666);
      ensure_ok(retval);
      ensure_ok(archive_write_header(ext_.get(), entry));

      while (true) {
        size_t size = 0;
        off_t offset = 0;
        const void *buffer = nullptr;
        int readret = ensure_ok(archive_read_data_block(
                        a_.get(), &buffer, &size, &offset));
        if (readret == ARCHIVE_EOF) {
          break;
        }
        ensure_ok(archive_write_data_block(
          ext_.get(), buffer, size, offset));
      }

      ensure_ok(archive_write_finish_entry(ext_.get()));
    }

    return (*this);
  }

  std::string rootfile() const
  {
    std::filesystem::path rootf(path_);
    rootf.replace_extension("")
         .replace_extension("");
    return rootf;
  }

private:
  struct archive_r_deleter final {
    void operator()(archive* a) const
    { archive_read_free(a); }
  };

  struct archive_w_deleter final {
    void operator()(archive* a) const
    { archive_write_free(a); }
  };

private:
  std::filesystem::path path_;
  std::unique_ptr<archive, archive_r_deleter> a_;
  std::unique_ptr<archive, archive_w_deleter> ext_;
};


std::string extract_map_archive(std::string path)
{
  return tarbz2_archive(std::move(path))
          .extract()
          .rootfile();
}


std::vector<import::region_paths> extract_osrm_packages(
  std::vector<import::region_paths> const& sources)
{
  std::mutex insertlock;
  std::vector<import::region_paths> output;
  std::for_each(std::execution::par_unseq,
    std::begin(sources), std::end(sources), 
    [&](auto const& source) {
      dbglog << "extracting region " << source.name;
      // all paths remain the same except the osrm uncompressed archive
      region_paths extracted_region(source);
      extracted_region.osrm = extract_map_archive(source.osrm);
      {
        std::scoped_lock<std::mutex> lk(insertlock);
        output.push_back(std::move(extracted_region));
      }
    });
    return output;
}

}