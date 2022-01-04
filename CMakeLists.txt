set(CMAKE_VERBOSE_MAKEFILE TRUE)

cmake_minimum_required(VERSION 3.16)

project(clio VERSION 0.1.0)

option(VERBOSE "Verbose build" TRUE)
if(VERBOSE)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
  set(FETCHCONTENT_QUIET FALSE CACHE STRING "Verbose FetchContent()")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.75 COMPONENTS filesystem log_setup log thread system REQUIRED)

include(ExternalProject)
include(FetchContent)
include(CMake/deps/rippled.cmake)
include(CMake/deps/cassandra.cmake)
include(CMake/deps/Postgres.cmake)
include(CMake/deps/gtest.cmake)

# configure_file(CMake/version-config.h include/version.h) # Not used, but an idea how to handle versioning.

add_library(clio)
target_include_directories(clio PUBLIC src)
target_link_libraries(clio PUBLIC cassandra ${PostgreSQL_LIBRARIES})

target_include_directories(clio PUBLIC ${rippled_SOURCE_DIR}/src ) # TODO: Seems like this shouldn't be needed?
target_link_libraries(clio PUBLIC xrpl_core grpc_pbufs)

set(CLIO_INSTALL_DIR /opt/clio)
set(CMAKE_INSTALL_PREFIX ${CLIO_INSTALL_DIR})

target_sources(clio PRIVATE
  ## Backend
  src/backend/CassandraBackend.cpp
  src/backend/PostgresBackend.cpp
  src/backend/BackendInterface.cpp
  src/backend/Pg.cpp
  src/backend/DBHelpers.cpp
  src/backend/SimpleCache.cpp
  src/backend/LayeredCache.cpp
  ## ETL
  src/etl/ETLSource.cpp
  src/etl/ReportingETL.cpp
  ## Subscriptions
  src/subscriptions/SubscriptionManager.cpp
  ## RPC
  src/rpc/RPC.cpp
  src/rpc/RPCHelpers.cpp
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


target_link_libraries(clio PUBLIC ${Boost_LIBRARIES})

add_executable(clio_server src/main.cpp)
target_link_libraries(clio_server PUBLIC clio)
install(TARGETS clio_server DESTINATION bin)

add_executable(clio_tests unittests/main.cpp)
target_link_libraries(clio_tests PUBLIC clio gtest_main)
# install(TARGETS clio_tests DESTINATION bin)

install(FILES example-config.json DESTINATION etc/clio)

gtest_discover_tests(clio_tests)
install (SCRIPT "${CMAKE_SOURCE_DIR}/CMake/PostInstall.cmake")
