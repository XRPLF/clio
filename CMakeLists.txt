#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

set(CMAKE_VERBOSE_MAKEFILE TRUE)
project(clio)
cmake_minimum_required(VERSION 3.16)
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wno-narrowing")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
if ((NOT DEFINED BOOST_ROOT) AND (DEFINED ENV{BOOST_ROOT}))
  set (BOOST_ROOT $ENV{BOOST_ROOT})
endif ()
file (TO_CMAKE_PATH "${BOOST_ROOT}" BOOST_ROOT)

FIND_PACKAGE( Boost 1.75 COMPONENTS filesystem log log_setup thread system REQUIRED )
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
FetchContent_MakeAvailable(googletest)
enable_testing()
include(GoogleTest)

add_executable (clio_server
    src/main.cpp
)
add_executable (clio_tests
    unittests/main.cpp
)
add_library(clio)
include_directories(src)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps")
include(ExternalProject)
message(${CMAKE_CURRENT_BINARY_DIR})
message(${CMAKE_MODULE_PATH})
add_subdirectory(rippled)
target_link_libraries(clio PUBLIC xrpl_core grpc_pbufs)
add_dependencies(clio xrpl_core)
add_dependencies(clio grpc_pbufs)
get_target_property(grpc_includes grpc_pbufs INCLUDE_DIRECTORIES)
message("${grpc_includes}")
message("${proto_includes}")
message("${SOURCE_DIR}")
INCLUDE_DIRECTORIES(${grpc_includes})
INCLUDE_DIRECTORIES(${SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${SOURCE_DIR}/include)
get_target_property(xrpl_core_includes xrpl_core INCLUDE_DIRECTORIES)
message("${xrpl_core_includes}")
INCLUDE_DIRECTORIES(${xrpl_core_includes})
include(cassandra)
include(Postgres)


target_sources(clio PRIVATE
## Reporting
    src/reporting/ETLSource.cpp
    src/reporting/CassandraBackend.cpp
    src/reporting/PostgresBackend.cpp
    src/reporting/BackendIndexer.cpp
    src/reporting/Pg.cpp
    src/reporting/P2pProxy.cpp
    src/reporting/DBHelpers.cpp
    src/reporting/ReportingETL.cpp
## Server
    src/server/SubscriptionManager.cpp
## RPC
    src/rpc/Status.cpp
    src/rpc/RPCHelpers.cpp
    src/rpc/Handlers.cpp
    src/rpc/Context.cpp
## Handlers
    # Account
    src/rpc/handlers/impl/AccountChannels.cpp
    src/rpc/handlers/impl/AccountCurrencies.cpp
    src/rpc/handlers/impl/AccountInfo.cpp
    src/rpc/handlers/impl/AccountLines.cpp
    src/rpc/handlers/impl/AccountOffers.cpp
    src/rpc/handlers/impl/AccountObjects.cpp
    # Ledger
    src/rpc/handlers/impl/Ledger.cpp
    src/rpc/handlers/impl/LedgerData.cpp
    src/rpc/handlers/impl/LedgerEntry.cpp
    src/rpc/handlers/impl/LedgerRange.cpp
    # Transaction
    src/rpc/handlers/impl/Tx.cpp
    src/rpc/handlers/impl/AccountTx.cpp
    # Dex
    src/rpc/handlers/impl/BookOffers.cpp
    # Payment Channel
    src/rpc/handlers/impl/ChannelAuthorize.cpp
    src/rpc/handlers/impl/ChannelVerify.cpp
    # Subscribe
    src/rpc/handlers/impl/Subscribe.cpp)


message(${Boost_LIBRARIES})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} ${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(clio PUBLIC ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(clio_server PUBLIC clio)
TARGET_LINK_LIBRARIES(clio_tests PUBLIC clio gtest_main)


gtest_discover_tests(clio_tests)

