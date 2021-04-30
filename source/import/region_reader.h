// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <fstream>
#include <optional>

#include "model/address.h"

namespace sentio::import
{
/**
 * An input iterator that is used to iterate over the region CSV file
 * that contains all known buildings within a region along with their
 * GPS coordinates. It is modeled after std::istream_iterator and yields
 * values of "building" type.
 *
 * notes:
 *  - silently skips over invalid rows
 */
class building_record_iterator
    : public std::iterator<std::input_iterator_tag, model::building>
{
public:  // construction
  /**
   * This constructor implements the end-of-stream iterator
   * that marks exhausting the underlying buildings csv file.
   */
  building_record_iterator() noexcept;

  /**
   * Initialize an instance of the iterator with an open
   * stream to the underlying csv file.
   */
  building_record_iterator(std::string const& filename);

  /**
   * Copy constructor.
   * The reason this copy constructor accepts a non-concst paramter is
   * because the stream needs to be set to the same offset as other's
   * position. The "tellg" method on the stream is a non-const member,
   * hence the unusual semantics of this copy constructor as well as
   * the == and != operators.
   */
  building_record_iterator(building_record_iterator& other);

public:  // record access
  /**
   * Returns the latest parsed CSV line of the world file.
   * Will crash if called on an invalidated iterator (such as
   * after eof is reached).
   */
  const model::building& operator*() const;
  const model::building* operator->() const;

  /**
   * Tests whether this operator is not invalidated.
   */
  operator bool() const;

public:  // cursor
  /**
   * Advances the stream past the next valid complete building
   * entry and stores the parsed row as a building in the iterator
   * so it can be dereferenced later on using the * operator.
   *
   * The format of a valid building row is as follows:
   *  321002253;18.5361181;54.5210699;PL;Gdynia;81-363;Starowiejska;35
   *
   * It happens sometimes that some rows are missing some of those
   * entries (such as zip code, lng, lat). This iterator will silently
   * skip over those rows and advance the cursor to the next valid value.
   */
  building_record_iterator operator++();

  /**
   * Tests if two iterators are pointing at the same position
   * in the underlying CSV file. It considers also eof state.
   */
  bool operator==(building_record_iterator& other);

  /**
   * Inverse of the == operator. It's implemented in terms
   * of negating the == comparision.
   */
  bool operator!=(building_record_iterator& other);

private:
  bool is_ok() const;

private:
  std::string filename_;
  std::optional<model::building> value_;
  std::optional<std::ifstream> stream_;
};

class region_addressbook
{
public:
  using iterator = building_record_iterator;

public:
  region_addressbook(std::string path);

public:
  iterator begin() const;
  iterator end() const;

public:
  std::string const& path() const;

private:
  std::string csvpath_;
};

}  // namespace sentio::import
