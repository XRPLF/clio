cmake_minimum_required(VERSION 3.16.3)

project(clio)

option(BUILD_TESTS "Build tests" TRUE)

option(VERBOSE "Verbose build" TRUE)
if(VERBOSE)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
  set(FETCHCONTENT_QUIET FALSE CACHE STRING "Verbose FetchContent()")
endif()

if(NOT GIT_COMMIT_HASH)
  if(VERBOSE)
    message(WARNING "GIT_COMMIT_HASH not provided...looking for git")
  endif()
  find_package(Git)
  if(Git_FOUND)
      execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --abbrev=8
          OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE gch)
      if(gch)
          set(GIT_COMMIT_HASH "${gch}")
          message(STATUS "Git commit: ${GIT_COMMIT_HASH}")
          add_definitions(-DCLIO_GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
      endif()
  endif()
endif() #git

add_library(clio)
target_compile_features(clio PUBLIC cxx_std_20)
target_include_directories(clio PUBLIC src)

include(FetchContent)
include(ExternalProject)
include(CMake/settings.cmake)
include(CMake/ClioVersion.cmake)
include(CMake/deps/rippled.cmake)
include(CMake/deps/Boost.cmake)
include(CMake/deps/cassandra.cmake)
include(CMake/deps/Postgres.cmake)

target_sources(clio PRIVATE
  ## Application
  src/clio/main/impl/Application.cpp
  src/clio/main/impl/Build.cpp
  src/clio/main/impl/Config.cpp
  ## Backend
  src/clio/backend/BackendInterface.cpp
  src/clio/backend/CassandraBackend.cpp
  src/clio/backend/Pg.cpp
  src/clio/backend/PostgresBackend.cpp
  src/clio/backend/SimpleCache.cpp
  ## ETL
  src/clio/etl/ETLSource.cpp
  src/clio/etl/NFTHelpers.cpp
  src/clio/etl/ReportingETL.cpp
  ## Subscriptions
  src/clio/subscriptions/SubscriptionManager.cpp
  ## RPC
  src/clio/rpc/RPC.cpp
  src/clio/rpc/RPCHelpers.cpp
  src/clio/rpc/Counters.cpp
  src/clio/rpc/WorkQueue.cpp
  ## Webserver
  src/clio/webserver/impl/DOSGuard.cpp
  src/clio/webserver/impl/Listener.cpp
  ## RPC Methods
  # Account
  src/clio/rpc/handlers/AccountChannels.cpp
  src/clio/rpc/handlers/AccountCurrencies.cpp
  src/clio/rpc/handlers/AccountInfo.cpp
  src/clio/rpc/handlers/AccountLines.cpp
  src/clio/rpc/handlers/AccountOffers.cpp
  src/clio/rpc/handlers/AccountObjects.cpp
  src/clio/rpc/handlers/GatewayBalances.cpp
  src/clio/rpc/handlers/NoRippleCheck.cpp
  # NFT
  src/clio/rpc/handlers/NFTInfo.cpp
  # Ledger
  src/clio/rpc/handlers/Ledger.cpp
  src/clio/rpc/handlers/LedgerData.cpp
  src/clio/rpc/handlers/LedgerEntry.cpp
  src/clio/rpc/handlers/LedgerRange.cpp
  # Transaction
  src/clio/rpc/handlers/Tx.cpp
  src/clio/rpc/handlers/TransactionEntry.cpp
  src/clio/rpc/handlers/AccountTx.cpp
  # Dex
  src/clio/rpc/handlers/BookOffers.cpp
  # NFT
  src/clio/rpc/handlers/NFTOffers.cpp
  # Payment Channel
  src/clio/rpc/handlers/ChannelAuthorize.cpp
  src/clio/rpc/handlers/ChannelVerify.cpp
  # Subscribe
  src/clio/rpc/handlers/Subscribe.cpp
  # Server
  src/clio/rpc/handlers/ServerInfo.cpp
  # Utility
  src/clio/rpc/handlers/Random.cpp)

add_executable(clio_server src/clio/main/main.cpp)
target_link_libraries(clio_server PUBLIC clio)

if(BUILD_TESTS)
  include(CMake/deps/gtest.cmake)

  add_library(test_lib)

  target_compile_features(test_lib PUBLIC cxx_std_20)

  target_sources(test_lib PUBLIC 
    src/test/backend/backend.Integration.cpp
    src/test/cache/Cache.backgroundLoad.cpp
    src/test/cache/Cache.basicCache.cpp
    src/test/etl/etl.networkValidatedLedgers.cpp
    src/test/etl/etl.threadSafeQueue.cpp
    src/test/etl/etl.getMarkers.cpp
    src/test/rpc/rpc.ledger.cpp
    src/test/rpc/rpc.accountInfo.cpp
    src/test/rpc/rpc.channelAuthorize.cpp
    src/test/rpc/rpc.channelVerify.cpp
    src/test/rpc/rpc.accountObjects.cpp
    src/test/rpc/rpc.accountLines.cpp
    src/test/rpc/rpc.accountChannels.cpp
    src/test/rpc/rpc.accountCurrencies.cpp
    src/test/rpc/rpc.accountOffers.cpp
    src/test/rpc/rpc.accountTx.cpp
    src/test/rpc/rpc.bookOffers.cpp
    src/test/rpc/rpc.ledgerData.cpp)

  target_link_libraries(test_lib PUBLIC clio gtest_main gmock_main)

  add_executable(clio_tests)
  target_link_libraries(clio_tests PUBLIC test_lib)
  gtest_discover_tests(clio_tests)

endif()

include(CMake/install/install.cmake)
if(PACKAGING)
    include(CMake/packaging.cmake)
endif()
