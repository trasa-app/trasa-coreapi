# Copyright (C) Karim Agha - All Rights Reserved
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>


include_directories(${PROJECT_SOURCE_DIR}/source)

function(add_unit_test SOURCE_FILE_NAME)
  get_filename_component(TEST_EXECUTABLE_NAME ${SOURCE_FILE_NAME} NAME_WE)
  add_executable(${TEST_EXECUTABLE_NAME} ${SOURCE_FILE_NAME} main.cc)
  target_link_libraries(${TEST_EXECUTABLE_NAME} trasa)
  add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND ${TEST_EXECUTABLE_NAME})
endfunction()


add_unit_test(poly_parser.cc)
add_unit_test(world_index.cc)
add_unit_test(street_index.cc)
add_unit_test(region_index.cc)