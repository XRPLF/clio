cmake_minimum_required(VERSION 3.16.3)

project(clio)

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
  message(FATAL_ERROR "GCC 11+ required for building clio")
endif()

option(BUILD_TESTS "Build tests" TRUE)

option(VERBOSE "Verbose build" TRUE)
if(VERBOSE)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
  set(FETCHCONTENT_QUIET FALSE CACHE STRING "Verbose FetchContent()")
endif()

if(NOT GIT_COMMIT_HASH)
  if(VERBOSE)
    message("GIT_COMMIT_HASH not provided...looking for git")
  endif()
  find_package(Git)
  if(Git_FOUND)
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
          OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE git-ref)
      if(git-ref)
          set(BUILD "${git-ref}")
          message(STATUS "Build version: ${BUILD}")
          add_definitions(-DCLIO_BUILD="${BUILD}")
      endif()
  endif()
endif() #git
if(PACKAGING)
  add_definitions(-DPKG=1)
endif()

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
include(CMake/deps/SourceLocation.cmake)

target_sources(clio PRIVATE
  ## Main
  src/main/impl/Build.cpp
  ## Backend
  src/backend/BackendInterface.cpp
  src/backend/CassandraBackend.cpp
  src/backend/SimpleCache.cpp
  ## ETL
  src/etl/ETLSource.cpp
  src/etl/ProbingETLSource.cpp
  src/etl/NFTHelpers.cpp
  src/etl/ReportingETL.cpp
  ## Subscriptions
  src/subscriptions/SubscriptionManager.cpp
  ## RPC
  src/rpc/Errors.cpp
  src/rpc/RPC.cpp
  src/rpc/RPCHelpers.cpp
  src/rpc/Counters.cpp
  src/rpc/WorkQueue.cpp
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
  # NFT
  src/rpc/handlers/NFTHistory.cpp
  src/rpc/handlers/NFTInfo.cpp
  src/rpc/handlers/NFTOffers.cpp
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
  src/rpc/handlers/BookChanges.cpp
  src/rpc/handlers/BookOffers.cpp
  # Payment Channel
  src/rpc/handlers/ChannelAuthorize.cpp
  src/rpc/handlers/ChannelVerify.cpp
  # Subscribe
  src/rpc/handlers/Subscribe.cpp
  # Server
  src/rpc/handlers/ServerInfo.cpp
  # Utilities
  src/rpc/handlers/Random.cpp
  src/config/Config.cpp
  src/log/Logger.cpp
  src/util/Taggable.cpp)

add_executable(clio_server src/main/main.cpp)
target_link_libraries(clio_server PUBLIC clio)

if(BUILD_TESTS)
  add_executable(clio_tests
    unittests/RPCErrors.cpp
    unittests/Backend.cpp
    unittests/Logger.cpp
    unittests/Config.cpp
    unittests/DOSGuard.cpp)
  include(CMake/deps/gtest.cmake)
endif()

include(CMake/install/install.cmake)
if(PACKAGING)
    include(CMake/packaging.cmake)
endif()
