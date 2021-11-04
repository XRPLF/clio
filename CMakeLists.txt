set(CMAKE_VERBOSE_MAKEFILE TRUE)

project(clio)
cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wno-narrowing")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.75 COMPONENTS filesystem log_setup log thread system REQUIRED)
find_package(ZLIB REQUIRED)

include(ExternalProject)
include(FetchContent)
include(deps/rippled.cmake)
include(deps/cassandra.cmake)
include(deps/Postgres.cmake)
include(deps/gtest.cmake)

configure_file(CMake/version-config.h include/version.h)

add_library(clio)
target_include_directories(clio PUBLIC src)
target_link_libraries(clio PUBLIC cassandra ${PostgreSQL_LIBRARIES})

target_include_directories(clio PUBLIC ${rippled_SOURCE_DIR}/src ) # TODO: xrpl_core should have transitive include dirs
target_link_libraries(clio PUBLIC xrpl_core grpc_pbufs)
# add_dependencies(clio xrpl_core) # TODO: This shouldn't be needed now that rippled is included through FetchContent()
# add_dependencies(clio grpc_pbufs)
# get_target_property(grpc_includes grpc_pbufs INCLUDE_DIRECTORIES)

message("GRPC includes: ${grpc_includes}")
message("Proto includes: ${proto_includes}")
message("Source dir: ${SOURCE_DIR}")
include_directories(${grpc_includes})
include_directories(${SOURCE_DIR}/src)
include_directories(${SOURCE_DIR}/include)

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


include_directories(${CMAKE_CURRENT_BINARY_DIR} ${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clio PUBLIC ${Boost_LIBRARIES})

add_executable(clio_server src/main.cpp)
target_link_libraries(clio_server PUBLIC clio)
install(TARGETS clio_server DESTINATION bin)

add_executable(clio_tests unittests/main.cpp)
target_link_libraries(clio_tests PUBLIC clio gtest_main)
# install(TARGETS clio_tests DESTINATION bin)

install(FILES example-config.json DESTINATION etc/clio)

gtest_discover_tests(clio_tests)
