#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

set(CMAKE_VERBOSE_MAKEFILE TRUE)
project(reporting)
cmake_minimum_required(VERSION 3.17)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
if ((NOT DEFINED BOOST_ROOT) AND (DEFINED ENV{BOOST_ROOT}))
  set (BOOST_ROOT $ENV{BOOST_ROOT})
endif ()
file (TO_CMAKE_PATH "${BOOST_ROOT}" BOOST_ROOT)

FIND_PACKAGE( Boost 1.75 COMPONENTS filesystem log log_setup thread system REQUIRED )

add_executable (reporting
    websocket_server_async.cpp
)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps")
include(ExternalProject)
message(${CMAKE_CURRENT_BINARY_DIR})
message(${CMAKE_MODULE_PATH})
#include(rippled/Builds/CMake/RippledCore.cmake)
add_subdirectory(rippled)
target_link_libraries(reporting PUBLIC xrpl_core grpc_pbufs)
add_dependencies(reporting xrpl_core)
add_dependencies(reporting grpc_pbufs)
get_target_property(grpc_includes grpc_pbufs INCLUDE_DIRECTORIES)
#get_target_property(xrpl_core_includes xrpl_core INCLUDE_DIRECTORIES)
get_target_property(proto_includes protobuf_src INCLUDE_DIRECTORIES)
message("hi")
message("${grpc_includes}")
message("${proto_includes}")
ExternalProject_Get_Property(protobuf_src SOURCE_DIR)
message("${SOURCE_DIR}")
INCLUDE_DIRECTORIES(${grpc_includes})
#INCLUDE_DIRECTORIES(${xrpl_core_includes})
INCLUDE_DIRECTORIES(${SOURCE_DIR}/src)
ExternalProject_Get_Property(grpc_src SOURCE_DIR)
INCLUDE_DIRECTORIES(${SOURCE_DIR}/include)
get_target_property(xrpl_core_includes xrpl_core INCLUDE_DIRECTORIES)
message("${xrpl_core_includes}")
INCLUDE_DIRECTORIES(${xrpl_core_includes})
include(cassandra)
include(Postgres)


target_sources(reporting PRIVATE
    reporting/ETLSource.cpp
    reporting/CassandraBackend.cpp
    reporting/PostgresBackend.cpp
    reporting/Pg.cpp
    reporting/DBHelpers.cpp
    reporting/ReportingETL.cpp
    handlers/AccountInfo.cpp
    handlers/Tx.cpp
    handlers/RPCHelpers.cpp
    handlers/AccountTx.cpp
    handlers/LedgerData.cpp
    handlers/BookOffers.cpp
    handlers/Ledger.cpp)


message(${Boost_LIBRARIES})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} ${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(reporting PUBLIC ${Boost_LIBRARIES})
