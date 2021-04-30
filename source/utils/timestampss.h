// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <iosfwd>
#include <streambuf>

namespace sentio::utils
{
/**
 * On startup atteched to std::cout, std::cerr and std::clog
 * prepends the current timestamp before every new line.
 */
class timestamped_streambuf : public std::streambuf
{
public:
  timestamped_streambuf(std::basic_ios<char>& out);
  ~timestamped_streambuf();

protected:
  traits_type::int_type overflow(int_type m = traits_type::eof());

private:  // not copyable
  timestamped_streambuf(const timestamped_streambuf&) = delete;
  timestamped_streambuf& operator=(const timestamped_streambuf&) = delete;

private:
  std::basic_ios<char>& out_;
  std::streambuf* sink_;
  bool newline_;
};

}