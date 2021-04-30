// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <chrono>
#include <string>
#include <sstream>

namespace sentio::geomodel
{

template<typename Rep, typename Period>
std::string print_duration(std::chrono::duration<Rep, Period> const& elapsed)
{
  namespace sc = std::chrono;
  std::stringstream out;
  auto diff = sc::duration_cast<sc::milliseconds>(elapsed).count();
  auto const msecs = diff % 1000; diff /= 1000;
  auto const secs = diff % 60; diff /= 60;
  auto const mins = diff % 60; diff /= 60;
  auto const hours = diff % 24; diff /= 24;
  auto const days = diff;

  bool printed_earlier = false;
  if (days >= 1) {
      printed_earlier = true;
      out << days << "d" << ' ';
  }
  if (printed_earlier || hours >= 1) {
      printed_earlier = true;
      out << hours << "h" << ' ';
  }
  if (printed_earlier || mins >= 1) {
      printed_earlier = true;
      out << mins << "m" << ' ';
  }
  if (printed_earlier || secs >= 1) {
      printed_earlier = true;
      out << secs << "s" << ' ';
  }
  if (printed_earlier || msecs >= 1) {
      printed_earlier = true;
      out << msecs << "ms";
  }
  return out.str();
}

}
