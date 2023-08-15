cmake_minimum_required(VERSION 3.16.3)
project(clio)

# ========================================================================== #
#                                   Options                                  #
# ========================================================================== #
option (verbose   "Verbose build"                                     FALSE) 
option (tests     "Build tests"                                       FALSE)
option (docs      "Generate doxygen docs"                             FALSE)
option (coverage  "Build test coverage report"                        FALSE)
option (packaging "Create distribution packages"                      FALSE)
# ========================================================================== #
set (san "" CACHE STRING "Add sanitizer instrumentation")
set_property (CACHE san PROPERTY STRINGS ";undefined;memory;address;thread")
# ========================================================================== #

# Include required modules
include (CheckCXXCompilerFlag)

if (verbose)
  set (CMAKE_VERBOSE_MAKEFILE TRUE)
endif ()

if (packaging)
  add_definitions (-DPKG=1)
endif ()

add_library (clio)

# Clio tweaks and checks
include (CMake/CheckCompiler.cmake)
include (CMake/Settings.cmake)
include (CMake/ClioVersion.cmake)
include (CMake/SourceLocation.cmake)

# Clio deps
include (CMake/deps/libxrpl.cmake)
include (CMake/deps/Boost.cmake)
include (CMake/deps/OpenSSL.cmake)
include (CMake/deps/Threads.cmake)
include (CMake/deps/libfmt.cmake)
include (CMake/deps/cassandra.cmake)

# TODO: Include directory will be wrong when installed.
target_include_directories (clio PUBLIC src)
target_compile_features (clio PUBLIC cxx_std_20)

target_link_libraries (clio
  PUBLIC Boost::boost
  PUBLIC Boost::coroutine
  PUBLIC Boost::program_options
  PUBLIC Boost::system
  PUBLIC Boost::log
  PUBLIC Boost::log_setup
  PUBLIC cassandra-cpp-driver::cassandra-cpp-driver
  PUBLIC fmt::fmt
  PUBLIC OpenSSL::Crypto
  PUBLIC OpenSSL::SSL
  PUBLIC xrpl::libxrpl

  INTERFACE Threads::Threads
)

if (is_gcc)
  # FIXME: needed on gcc for now
  target_compile_definitions (clio PUBLIC BOOST_ASIO_DISABLE_CONCEPTS)
endif ()

target_sources (clio PRIVATE
  ## Main
  src/main/impl/Build.cpp
  ## Backend
  src/data/BackendInterface.cpp
  src/data/LedgerCache.cpp
  src/data/cassandra/impl/Future.cpp
  src/data/cassandra/impl/Cluster.cpp
  src/data/cassandra/impl/Batch.cpp
  src/data/cassandra/impl/Result.cpp
  src/data/cassandra/impl/Tuple.cpp
  src/data/cassandra/impl/SslContext.cpp
  src/data/cassandra/Handle.cpp
  src/data/cassandra/SettingsProvider.cpp
  ## ETL
  src/etl/Source.cpp
  src/etl/ProbingSource.cpp
  src/etl/NFTHelpers.cpp
  src/etl/ETLService.cpp
  src/etl/LoadBalancer.cpp
  src/etl/impl/ForwardCache.cpp
  ## Feed
  src/feed/SubscriptionManager.cpp
  ## Web
  src/web/IntervalSweepHandler.cpp
  ## RPC
  src/rpc/Errors.cpp
  src/rpc/Factories.cpp
  src/rpc/RPCHelpers.cpp
  src/rpc/Counters.cpp
  src/rpc/WorkQueue.cpp
  src/rpc/common/Specs.cpp
  src/rpc/common/Validators.cpp
  src/rpc/common/MetaProcessors.cpp
  src/rpc/common/impl/APIVersionParser.cpp
  src/rpc/common/impl/HandlerProvider.cpp
  ## RPC handlers
  src/rpc/handlers/AccountChannels.cpp
  src/rpc/handlers/AccountCurrencies.cpp
  src/rpc/handlers/AccountInfo.cpp
  src/rpc/handlers/AccountLines.cpp
  src/rpc/handlers/AccountNFTs.cpp
  src/rpc/handlers/AccountObjects.cpp
  src/rpc/handlers/AccountOffers.cpp
  src/rpc/handlers/AccountTx.cpp
  src/rpc/handlers/BookChanges.cpp
  src/rpc/handlers/BookOffers.cpp
  src/rpc/handlers/DepositAuthorized.cpp
  src/rpc/handlers/GatewayBalances.cpp
  src/rpc/handlers/Ledger.cpp
  src/rpc/handlers/LedgerData.cpp
  src/rpc/handlers/LedgerEntry.cpp
  src/rpc/handlers/LedgerRange.cpp
  src/rpc/handlers/NFTBuyOffers.cpp
  src/rpc/handlers/NFTHistory.cpp
  src/rpc/handlers/NFTInfo.cpp
  src/rpc/handlers/NFTOffersCommon.cpp
  src/rpc/handlers/NFTSellOffers.cpp
  src/rpc/handlers/NoRippleCheck.cpp
  src/rpc/handlers/Random.cpp
  src/rpc/handlers/TransactionEntry.cpp
  src/rpc/handlers/Tx.cpp
  ## Util
  src/util/config/Config.cpp
  src/util/log/Logger.cpp
  src/util/Taggable.cpp)

# Clio server
add_executable (clio_server src/main/Main.cpp)
target_link_libraries (clio_server PUBLIC clio)

# Unittesting
if (tests)
  set (TEST_TARGET clio_tests)
  add_executable (${TEST_TARGET}
    # Common
    unittests/Main.cpp
    unittests/Playground.cpp
    unittests/LoggerTests.cpp
    unittests/ConfigTests.cpp
    unittests/ProfilerTests.cpp
    unittests/JsonUtilTests.cpp
    unittests/DOSGuardTests.cpp
    unittests/SubscriptionTests.cpp
    unittests/SubscriptionManagerTests.cpp
    unittests/util/TestObject.cpp
    unittests/util/StringUtils.cpp
    # ETL
    unittests/etl/ExtractionDataPipeTests.cpp
    unittests/etl/ExtractorTests.cpp
    unittests/etl/TransformerTests.cpp
    unittests/etl/CacheLoaderTests.cpp
    # RPC
    unittests/rpc/ErrorTests.cpp
    unittests/rpc/BaseTests.cpp
    unittests/rpc/RPCHelpersTests.cpp
    unittests/rpc/CountersTests.cpp
    unittests/rpc/AdminVerificationTests.cpp
    unittests/rpc/APIVersionTests.cpp
    unittests/rpc/ForwardingProxyTests.cpp
    unittests/rpc/WorkQueueTests.cpp
    unittests/rpc/AmendmentsTests.cpp
    ## RPC handlers
    unittests/rpc/handlers/DefaultProcessorTests.cpp
    unittests/rpc/handlers/TestHandlerTests.cpp
    unittests/rpc/handlers/AccountCurrenciesTests.cpp
    unittests/rpc/handlers/AccountLinesTests.cpp
    unittests/rpc/handlers/AccountTxTests.cpp
    unittests/rpc/handlers/AccountOffersTests.cpp
    unittests/rpc/handlers/AccountInfoTests.cpp
    unittests/rpc/handlers/AccountChannelsTests.cpp
    unittests/rpc/handlers/AccountNFTsTests.cpp
    unittests/rpc/handlers/BookOffersTests.cpp
    unittests/rpc/handlers/DepositAuthorizedTests.cpp
    unittests/rpc/handlers/GatewayBalancesTests.cpp
    unittests/rpc/handlers/TxTests.cpp
    unittests/rpc/handlers/TransactionEntryTests.cpp
    unittests/rpc/handlers/LedgerEntryTests.cpp
    unittests/rpc/handlers/LedgerRangeTests.cpp
    unittests/rpc/handlers/NoRippleCheckTests.cpp
    unittests/rpc/handlers/ServerInfoTests.cpp
    unittests/rpc/handlers/PingTests.cpp
    unittests/rpc/handlers/RandomTests.cpp
    unittests/rpc/handlers/NFTInfoTests.cpp
    unittests/rpc/handlers/NFTBuyOffersTests.cpp
    unittests/rpc/handlers/NFTSellOffersTests.cpp
    unittests/rpc/handlers/NFTHistoryTests.cpp
    unittests/rpc/handlers/SubscribeTests.cpp
    unittests/rpc/handlers/UnsubscribeTests.cpp
    unittests/rpc/handlers/LedgerDataTests.cpp
    unittests/rpc/handlers/AccountObjectsTests.cpp
    unittests/rpc/handlers/BookChangesTests.cpp
    unittests/rpc/handlers/LedgerTests.cpp
    unittests/rpc/handlers/VersionHandlerTests.cpp
    # Backend
    unittests/data/BackendFactoryTests.cpp
    unittests/data/cassandra/BaseTests.cpp
    unittests/data/cassandra/BackendTests.cpp
    unittests/data/cassandra/RetryPolicyTests.cpp
    unittests/data/cassandra/SettingsProviderTests.cpp
    unittests/data/cassandra/ExecutionStrategyTests.cpp
    unittests/data/cassandra/AsyncExecutorTests.cpp
    # Webserver
    unittests/web/ServerTests.cpp
    unittests/web/RPCServerHandlerTests.cpp
    unittests/web/WhitelistHandlerTests.cpp
    unittests/web/SweepHandlerTests.cpp)

  include (CMake/deps/gtest.cmake)

  # See https://github.com/google/googletest/issues/3475
  gtest_discover_tests (clio_tests DISCOVERY_TIMEOUT 10)

  # Fix for dwarf5 bug on ci 
  target_compile_options (clio PUBLIC -gdwarf-4)

  target_compile_definitions (${TEST_TARGET} PUBLIC UNITTEST_BUILD)
  target_include_directories (${TEST_TARGET} PRIVATE unittests)
  target_link_libraries (${TEST_TARGET} PUBLIC clio gtest::gtest)

  # Generate `clio_tests-ccov` if coverage is enabled
  # Note: use `make clio_tests-ccov` to generate report
  if (coverage)
    include (CMake/Coverage.cmake)
    add_coverage (${TEST_TARGET})
  endif ()
endif ()

# Enable selected sanitizer if enabled via `san`
if (san)
  target_compile_options (clio
    PUBLIC
      # Sanitizers recommend minimum of -O1 for reasonable performance
      $<$<CONFIG:Debug>:-O1>
      ${SAN_FLAG}
      -fno-omit-frame-pointer)
  target_compile_definitions (clio
    PUBLIC
      $<$<STREQUAL:${san},address>:SANITIZER=ASAN>
      $<$<STREQUAL:${san},thread>:SANITIZER=TSAN>
      $<$<STREQUAL:${san},memory>:SANITIZER=MSAN>
      $<$<STREQUAL:${san},undefined>:SANITIZER=UBSAN>)
  target_link_libraries (clio INTERFACE ${SAN_FLAG} ${SAN_LIB})
endif ()

# Generate `docs` target for doxygen documentation if enabled
# Note: use `make docs` to generate the documentation
if (docs)
  include (CMake/Docs.cmake)
endif ()

include (CMake/install/install.cmake)
if (packaging)
  include (CMake/packaging.cmake) # This file exists only in build runner
endif ()
