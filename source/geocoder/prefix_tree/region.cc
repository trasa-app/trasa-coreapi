// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <cassert>
#include <sqlite3.h>

#include "region.h"
#include "street.h"

namespace sentio::geocoder
{

/**
 * This region index implementation is based on an in-memory
 * radix tree implementation supplied by GNU PBDS extensions.
 * 
 * This implementation is not very memory efficient.
 */
class region::impl
{
public:
  impl(std::string name)
    : count_(0)
    , name_(std::move(name))
  {
  }

public:
  size_t size() const
  { return count_; }

  std::string name() const
  { return name_; }

  void seal() const {}

public:
  region::iterator begin() const
  { return streets_.begin(); }

  region::iterator end() const
  { return streets_.end(); }

  region::iterator find(std::string const& exact) const
  {
    assert(!exact.empty());
    return streets_.find(exact);
  }

  region::range prefix_match(std::string const& prefix) const
  {
    assert(!prefix.empty());
    return streets_.prefix_range(prefix);
  }

  void insert(model::building b)
  {
    assert(!b.city.empty());
    assert(!b.number.empty());
    assert(!b.street.empty());
    assert(!b.coords.empty());

    // check if we have a street with this name
    if (auto existing_s = streets_.find(b.street); existing_s != streets_.end()) {
      // check if we have a city that has a street with this name
      if (auto existing_c = existing_s->second.find(b.city);
          existing_c != existing_s->second.end()) {
        existing_c->second.insert(std::move(b));  // street and city already known
      } else {  // steet known but not in this city
        std::string city = b.city;
        street ix(b.street);
        ix.insert(std::move(b));
        existing_s->second.emplace(
          std::move(city), std::move(ix));
      }
    } else {
      std::string city = b.city;
      std::string street = b.street;
      geocoder::street ix(b.street);
      ix.insert(std::move(b));
      region::mapped_type street_city;
      street_city.emplace(city, std::move(ix));
      streets_.insert(
        std::make_pair(
          std::move(street), 
          std::move(street_city)));
    }
    ++count_;
  }

private:
  size_t count_;
  std::string name_;
  region::trie streets_;
};

//
// public interface
//

// compiler-generated members
// this is to support clean pimpl idiom with unique_ptr without
// exposing the implementation details of the underlying impl type.

region::~region() = default;
region::region(region&&) = default;
region& region::operator=(region&&) = default;

region::region(std::string name)
  : impl_(std::make_unique<impl>(std::move(name)))
{
}

size_t region::size() const 
{ return impl_->size(); }

std::string region::name() const 
{ return impl_->name(); }

region::iterator region::begin() const
{ return impl_->begin(); }

region::iterator region::end() const 
{ return impl_->end(); }

region::iterator region::find(std::string const& exact) const
{ return impl_->find(exact); }

region::range region::prefix_match(std::string const& prefix) const
{ return impl_->prefix_match(prefix); }

void region::insert(model::building b)
{ return impl_->insert(std::move(b)); }

void region::seal()
{ return impl_->seal(); }

}  // namespace sentio::kurier
