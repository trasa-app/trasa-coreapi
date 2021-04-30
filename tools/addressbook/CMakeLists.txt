# Copyright (C) Karim Agha - All Rights Reserved
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

cmake_minimum_required(VERSION 3.16)

add_executable(addressbook
  main.cc)

target_link_libraries(addressbook
  dl trasa
  ${Boost_LIBRARIES}
  ${SQLite3_LIBRARIES})