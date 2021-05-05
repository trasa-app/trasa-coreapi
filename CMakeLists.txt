# Copyright (C) Karim Agha - All Rights Reserved
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

cmake_minimum_required(VERSION 3.20)
project(turbo_trasa 
  VERSION 1.0
  HOMEPAGE_URL http://trasa.app
  LANGUAGES C CXX
  DESCRIPTION "Backend services for trasa")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR AND NOT MSVC_IDE)
  message(FATAL_ERROR "In-source builds are not allowed.
Please create a directory and run cmake from there, passing the path to this source directory as the last argument.
This process created the file `CMakeCache.txt' and the directory `CMakeFiles'. Please delete them.")
endif()

#--------------
# CMAKE Options
#--------------

# auto import custom cmake scripts
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#----------------------------
# Environment & CMAKE options
#----------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -frtti")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(other_flags "${other_flags} -frtti -fvisibility-inlines-hidden")
  set(other_flags "${other_flags} -fvisibility=hidden")
  add_definitions(-DBOOST_STACKTRACE_USE_BACKTRACE)
  add_definitions(-DBOOST_STACKTRACE_USE_ADDR2LINE)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${other_flags}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if (TRASA_DOCKER_BUILD)
  add_definitions(-DDOCKER_BUILD)
endif()

#----------------------
# External dependencies
#----------------------

set(Boost_USE_STATIC_LIBS   TRUE)
set(Boost_USE_RELEASE_LIBS  TRUE)
set(Boost_USE_MULTITHREADED TRUE)
set(OPENSSL_USE_STATIC_LIBS TRUE)


find_package(PkgConfig)
find_package(CURL REQUIRED)
find_package(Backtrace)
find_package(Torch REQUIRED)
find_package(LibArchive REQUIRED MODULE)
find_package(ZLIB REQUIRED MODULE)
find_package(OpenSSL REQUIRED)
find_package(LibOSRM REQUIRED)
find_package(TBB REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(AWSSDK REQUIRED 
  COMPONENTS dynamodb monitoring  events logs sqs s3)
find_package(Boost 1.71 REQUIRED
  COMPONENTS 
      system random chrono atomic
      regex filesystem program_options
      iostreams thread date_time log log_setup
      unit_test_framework locale)

# various hacks and workarounds...
add_definitions(-D_GNU_SOURCE)
add_definitions(-DBOOST_ASIO_DISABLE_CONCEPTS)
add_definitions(-DBOOST_THREAD_PROVIDES_FUTURE)
add_definitions(-DBOOST_THREAD_PROVIDES_FUTURE_CONTINUATION)
add_definitions(-DBOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
add_definitions(-DBOOST_ALLOW_DEPRECATED_HEADERS)
add_definitions(-DTBB_SUPPRESS_DEPRECATED_MESSAGES)
# end of various hacks and workarounds...

#--------------------
# Global includes
#--------------------

include_directories(${BOOST_URL_HEADERS})
include_directories(${BOOST_JSON_HEADERS})
include_directories(${TORCH_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SQLite3_INCLUDE_DIRS})
include_directories(${LibArchive_INCLUDE_DIRS})
include_directories(${Backtrace_INCLUDE_DIRS})
include_directories(${AWSSDK_INCLUDE_DIRS})
include_directories(${TBB_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${LibOSRM_INCLUDE_DIR})
include_directories(${LibOSRM_INCLUDE_DIR}/osrm)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/source)

#----------------
# Compile Sources
#----------------

#
# The build targets are split into a library and an executable
# so that library code could be unit tested by linking aginst it
# from the test executables.
#

add_library(trasa
  source/rpc/error.cc
  source/rpc/server.cc
  source/rpc/jwt.cc
  
  source/account/model.cc
  source/account/store.cc

  source/routing/trip.cc
  source/routing/worker.cc
  source/routing/config.cc
  source/routing/waypoint.cc
  source/routing/scheduler.cc
  source/routing/osrm_interop.cc

  source/spacial/index.cc 
  source/spacial/region.cc
  source/spacial/coords.cc

  source/import/map_source.cc
  source/import/osrm_archive.cc
  source/import/region_reader.cc

  source/geocoder/geocoder.cc
  source/geocoder/ner/geomodel.cc
  source/geocoder/sqlite_fts/sqlite_fts.cc

  source/tracking/event.cc

  source/services/trip.cc
  source/services/geocoder.cc
  source/services/accounts.cc
  source/services/distance.cc

  source/model/address.cc

  source/utils/aws.cc
  source/utils/datetime.cc)

#----------------
# Compile assets
#----------------

# pretrained addresses NER neural model
add_custom_target(poland_model_ner
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ld -r -b binary assets/poland.model -o ${CMAKE_BINARY_DIR}/CMakeFiles/trasa.dir/source/geocoder/ner/poland_model.o)
add_dependencies(trasa poland_model_ner)

#----------------
# Link binaries
#----------------

target_link_libraries(trasa
  dl backtrace
  OpenSSL::SSL
  OpenSSL::Crypto
  ${TBB_LIBRARIES}
  ${AWSSDK_LIBRARIES}
  ${TORCH_LIBRARIES}
  ${SQLite3_LIBRARIES}
  ${LibArchive_LIBRARIES}
  ${Boost_LIBRARIES}  
  ${Backtrace_LIBRARY}
  ${ZLIB_LIBRARIES}
  
  # embedded assets
  ${CMAKE_BINARY_DIR}/CMakeFiles/trasa.dir/source/geocoder/ner/poland_model.o)

#
# This is the main daemon executable, almost all its functionality
# is implemented in the "kurier" library, except capturing env config
# from the command line. This is essentially the main() function that 
# calls into kurier.
#

# the daemon executable
add_executable(turbo_server
  source/main.cc)

target_link_libraries(turbo_server
  dl trasa
  ${AWSSDK_LIBRARIES}
  ${LibArchive_LIBRARIES}
  ${Backtrace_LIBRARY} 
  ${SQLite3_LIBRARIES}
  ${TORCH_LIBRARIES}
  ${TBB_LIBRARIES}
  ${LibOSRM_LIBRARIES}
  ${Boost_LIBRARIES})


#------------------------------------
# Assemble final product deliverables
#------------------------------------

# Polish map regions deFinition
configure_file(
  assets/config.dev.json 
  ${CMAKE_BINARY_DIR}/product/config.dev.json 
  COPYONLY)

configure_file(
  assets/config.micro.json 
  ${CMAKE_BINARY_DIR}/product/config.micro.json 
  COPYONLY)

configure_file(
  assets/config.prod.json 
  ${CMAKE_BINARY_DIR}/product/config.prod.json 
  COPYONLY)

# Server executable binary
add_custom_command(TARGET turbo_server POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy 
    ${CMAKE_BINARY_DIR}/turbo_server
    ${CMAKE_BINARY_DIR}/product/turbo_server)

add_custom_command(TARGET turbo_server POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E cmake_echo_color --yellow
  "Final product saved in ${CMAKE_BINARY_DIR}/product/")

#---------------
# Unit Tests
#---------------

# enable_testing()
# add_subdirectory(test)


#---------------
# Internal Tools
#---------------

add_subdirectory(tools/geomodel)
add_subdirectory(tools/addressbook)