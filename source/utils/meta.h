// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <functional>
#include <boost/optional.hpp>

/**
 * Used mostly to maintain input argument invariants to functions.
 */
#define verify_argument(expr)                                      \
  do {                                                             \
    if (!(expr)) {                                                 \
      throw std::invalid_argument("argument test failed: " #expr); \
    }                                                              \
  } while (0)


template <typename T>
inline std::optional<T> to_std(boost::optional<T>&& v)
{ 
  if (v.has_value()) {
    return { std::move(v.value()) };
  }
  return {};
}

struct scope_exit {
  scope_exit(std::function<void (void)> f) : f_(std::move(f)) {}
  ~scope_exit(void) { f_(); }
private:
  std::function<void (void)> f_;
};

template <typename Impl>
class movable_only
{
public:
  movable_only(movable_only const&) = delete;
  movable_only& operator=(movable_only const&) = delete;
public:
  movable_only(movable_only&&) = default;
  movable_only& operator=(movable_only&&) = default;
};