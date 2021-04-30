// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <cassert>
#include <sstream>
#include <filesystem>
#include <boost/lexical_cast.hpp>

#include "region_reader.h"

namespace sentio::import
{
building_record_iterator::building_record_iterator() noexcept
    : stream_(nullptr)
{
}

building_record_iterator::building_record_iterator(std::string const& filename)
    : filename_(filename)
    , stream_(filename_)
{
  if (!std::filesystem::exists(filename_)) {
    throw std::invalid_argument("region file does not exist.");
  }
  this->operator++();  // ingest first value
}

building_record_iterator::building_record_iterator(
    building_record_iterator& other)
    : filename_(other.filename_)
    , value_(other.value_)
{
  if (other.stream_.has_value() && !other.filename_.empty()) {
    stream_ = std::ifstream(filename_);
    stream_->seekg(other.stream_->tellg());
  }
}

const model::building& building_record_iterator::operator*() const
{
  assert(value_.has_value());
  return *value_;
}

const model::building* building_record_iterator::operator->() const
{
  assert(value_.has_value());
  return &(*value_);
}

building_record_iterator::operator bool() const { return is_ok(); }

building_record_iterator building_record_iterator::operator++()
{
  while (true) {
    std::string line, cell;
    std::getline(*stream_, line);
    std::vector<std::string> chunks;
    std::stringstream ss(std::move(line));

    if (!is_ok()) {
      // close underlying streams and put this iterator in a state
      // that will make it equal to the past-the-end iterator, so
      // any loops that iterate over it would stop.
      value_.reset();
      stream_.reset();
      return (*this);
    }

    while (std::getline(ss, cell, ';')) {
      chunks.push_back(std::move(cell));
    }

    if (chunks.size() != 8) {
      continue;  // invalid/incomplete row. skip
    }

    try {
      model::building temp{
          boost::lexical_cast<int64_t>(chunks[0]),                     // id
          spacial::coordinates{boost::lexical_cast<double>(chunks[1]),   // lat
                             boost::lexical_cast<double>(chunks[2])},  // lng
          chunks[3],  // country
          chunks[4],  // city
          chunks[5],  // zipcode
          chunks[6],  // street
          chunks[7]   // building number
      };

      // if any of the key fields is missing skip this row and keep trying
      // to ingest the next row until a valid one is parsed or eof is reached.
      if (temp.coords.empty() || temp.city.empty() || temp.street.empty() ||
          temp.number.empty()) {
        continue;
      }

      // otherwise store it as the current value
      // for subsequent calls to operator* and ->
      value_.emplace(std::move(temp));

      // got a valid row, stop iterating until the user requests the next row
      // using another call to operator++.
      break;
    } catch (...) {
      continue;  // parsing row failed, skip and go to next row
    }
  }

  return (*this);
}

bool building_record_iterator::operator==(building_record_iterator& other)
{
  if (!stream_.has_value() && !other.stream_.has_value()) {
    return true;
  }

  if ((stream_.has_value() && other.stream_.has_value()) ||
      (!stream_.has_value() && !other.stream_.has_value())) {
    return false;
  }

  auto pos1 = this->stream_->tellg();
  auto pos2 = other.stream_->tellg();
  return pos1 == pos2;
}

bool building_record_iterator::operator!=(building_record_iterator& other)
{
  return !((*this) == other);
}

bool building_record_iterator::is_ok() const
{
  return stream_.has_value() && stream_->good() && !stream_->eof();
}

region_addressbook::region_addressbook(std::string csvpath)
    : csvpath_(std::move(csvpath))
{
}

region_addressbook::iterator region_addressbook::begin() const
{
  return building_record_iterator(csvpath_);
}

std::string const& region_addressbook::path() const
{ return csvpath_; }

region_addressbook::iterator region_addressbook::end() const
{
  return building_record_iterator();
}
}  // namespace sentio::import