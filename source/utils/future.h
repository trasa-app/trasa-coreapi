// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

namespace sentio::utils
{
// for std::future
template <typename It>
void wait_for_all(It beg, It end)
{
  for (auto it = beg; it != end; ++it) {
    it->wait();
  }
}

template <typename... Fut>
void wait_for_all(Fut&... futures) {
  bool dummy[] = { (futures.wait(), true)... };
  boost::ignore_unused(dummy);
}

template <typename It>
void join_all(It begin, It end) {
  static_assert(std::is_same<typename It::value_type, std::thread>::value);
  for (It it = begin; it != end; ++it) {
    it->join();
  }
}
}  // namespace sentio::utils