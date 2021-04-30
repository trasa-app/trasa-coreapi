# Copyright (C) Karim Agha - All Rights Reserved
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

cmake_minimum_required(VERSION 3.16)

add_executable(geomodel
  transform.cc
  dataset.cc
  train.cc
  main.cc)

target_link_libraries(geomodel
  dl trasa
  ${TORCH_LIBRARIES}
  ${Boost_LIBRARIES})