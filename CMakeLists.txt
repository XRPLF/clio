cmake_minimum_required(VERSION 3.16)

project(clio VERSION 0.1.0)

option(BUILD_TESTS "Build tests" TRUE)

option(VERBOSE "Verbose build" TRUE)
if(VERBOSE)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
  set(FETCHCONTENT_QUIET FALSE CACHE STRING "Verbose FetchContent()")
endif()

add_library(clio)
target_include_directories(clio PUBLIC src)


include(FetchContent)
if(${CMAKE_VERSION} VERSION_LESS "3.20.0")
  include(ExternalProject)
endif()
include(CMake/settings.cmake)
include(CMake/deps/rippled.cmake)
include(CMake/deps/Boost.cmake)
include(CMake/deps/cassandra.cmake)
include(CMake/deps/Postgres.cmake)

# configure_file(CMake/version-config.h include/version.h) # NOTE: Not used, but an idea how to handle versioning.

target_sources(clio PRIVATE
  ## Backend
  src/backend/BackendInterface.cpp
  src/backend/CassandraBackend.cpp
  src/backend/DBHelpers.cpp
  src/backend/LayeredCache.cpp
  src/backend/Pg.cpp
  src/backend/PostgresBackend.cpp
  src/backend/SimpleCache.cpp
  ## ETL
  src/etl/ETLSource.cpp
  src/etl/NFTHelpers.cpp
  src/etl/ReportingETL.cpp
  ## Subscriptions
  src/subscriptions/SubscriptionManager.cpp
  ## RPC
  src/rpc/RPC.cpp
  src/rpc/RPCHelpers.cpp
  src/rpc/Counters.cpp
  ## RPC Methods
  # Account
  src/rpc/handlers/AccountChannels.cpp
  src/rpc/handlers/AccountCurrencies.cpp
  src/rpc/handlers/AccountInfo.cpp
  src/rpc/handlers/AccountLines.cpp
  src/rpc/handlers/AccountOffers.cpp
  src/rpc/handlers/AccountObjects.cpp
  src/rpc/handlers/GatewayBalances.cpp
  src/rpc/handlers/NoRippleCheck.cpp
  # Ledger
  src/rpc/handlers/Ledger.cpp
  src/rpc/handlers/LedgerData.cpp
  src/rpc/handlers/LedgerEntry.cpp
  src/rpc/handlers/LedgerRange.cpp
  # Transaction
  src/rpc/handlers/Tx.cpp
  src/rpc/handlers/TransactionEntry.cpp
  src/rpc/handlers/AccountTx.cpp
  # Dex
  src/rpc/handlers/BookOffers.cpp
  # Payment Channel
  src/rpc/handlers/ChannelAuthorize.cpp
  src/rpc/handlers/ChannelVerify.cpp
  # Subscribe
  src/rpc/handlers/Subscribe.cpp
  # Server
  src/rpc/handlers/ServerInfo.cpp
  # Utility
  src/rpc/handlers/Random.cpp)

add_executable(clio_server src/main.cpp)
target_link_libraries(clio_server PUBLIC clio)

if(BUILD_TESTS)
  add_executable(clio_tests unittests/main.cpp)
  include(CMake/deps/gtest.cmake)
endif()

include(CMake/install/install.cmake)
