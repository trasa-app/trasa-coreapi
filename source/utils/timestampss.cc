// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <ctime>
#include <cassert>
#include <iomanip>
#include <iostream>

#include "timestampss.h"

namespace sentio::utils
{
timestamped_streambuf::timestamped_streambuf(std::basic_ios<char>& out)
    : out_(out)
    , sink_()
    , newline_(true)
{
  sink_ = out_.rdbuf(this);
  assert(sink_);
}

timestamped_streambuf::~timestamped_streambuf() { out_.rdbuf(sink_); }

timestamped_streambuf::traits_type::int_type timestamped_streambuf::overflow(
    timestamped_streambuf::traits_type::int_type m)
{
  if (traits_type::eq_int_type(m, traits_type::eof()))
    return sink_->pubsync() == -1 ? m : traits_type::not_eof(m);
  if (newline_) {
    std::ostream str(sink_);
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    if (!(str << std::put_time(&tm, "[%d-%m %H:%M:%S] ")))
      return traits_type::eof();
  }
  newline_ = traits_type::to_char_type(m) == '\n';
  return sink_->sputc(m);
}
}  // namespace sentio::utils