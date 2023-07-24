cmake_minimum_required(VERSION 3.16.3)
project(clio)

# ==================================================== #
#                       Options                        #
# ==================================================== #
option(verbose   "Verbose build"                FALSE) 
option(tests     "Build tests"                  FALSE)
option(coverage  "Build test coverage report"   FALSE)
option(packaging "Create distribution packages" FALSE)
# ==================================================== #

if(verbose)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
endif()

if(packaging)
  add_definitions(-DPKG=1)
endif()

#c++20 removed std::result_of but boost 1.75 is still using it.
add_definitions(-DBOOST_ASIO_HAS_STD_INVOKE_RESULT=1)

add_library(clio)

# Clio tweaks and checks
include(CMake/CheckCompiler.cmake)
include(CMake/Settings.cmake)
include(CMake/ClioVersion.cmake)
include(CMake/SourceLocation.cmake)

# Clio deps
include(CMake/deps/libxrpl.cmake)
include(CMake/deps/Boost.cmake)
include(CMake/deps/OpenSSL.cmake)
include(CMake/deps/Threads.cmake)
include(CMake/deps/libfmt.cmake)
include(CMake/deps/cassandra.cmake)

# TODO: Include directory will be wrong when installed.
target_include_directories(clio PUBLIC src)
target_compile_features(clio PUBLIC cxx_std_20)

target_link_libraries(clio
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

target_compile_definitions(clio PUBLIC 
  BOOST_ASIO_DISABLE_HANDLER_TYPE_REQUIREMENTS 
  BOOST_ASIO_HAS_STD_INVOKE_RESULT 
  BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT
  BOOST_BEAST_ALLOW_DEPRECATED
  BOOST_CONTAINER_FWD_BAD_DEQUE
  BOOST_COROUTINES_NO_DEPRECATION_WARNING)

target_sources(clio PRIVATE
  ## Main
  src/main/impl/Build.cpp
  ## Backend
  src/backend/BackendInterface.cpp
  src/backend/LedgerCache.cpp
  src/backend/cassandra/impl/Future.cpp
  src/backend/cassandra/impl/Cluster.cpp
  src/backend/cassandra/impl/Batch.cpp
  src/backend/cassandra/impl/Result.cpp
  src/backend/cassandra/impl/Tuple.cpp
  src/backend/cassandra/impl/SslContext.cpp
  src/backend/cassandra/Handle.cpp
  src/backend/cassandra/SettingsProvider.cpp
  ## ETL
  src/etl/Source.cpp
  src/etl/ProbingSource.cpp
  src/etl/NFTHelpers.cpp
  src/etl/ETLService.cpp
  src/etl/LoadBalancer.cpp
  src/etl/impl/ForwardCache.cpp
  ## Subscriptions
  src/subscriptions/SubscriptionManager.cpp
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
  src/config/Config.cpp
  src/log/Logger.cpp
  src/util/Taggable.cpp)

# Clio server
add_executable(clio_server src/main/Main.cpp)
target_link_libraries(clio_server PUBLIC clio)

# Unittesting
if(tests)
  set(TEST_TARGET clio_tests)
  add_executable(${TEST_TARGET}
    # Common
    unittests/Main.cpp
    unittests/Playground.cpp
    unittests/Logger.cpp
    unittests/Config.cpp
    unittests/ProfilerTest.cpp
    unittests/JsonUtilTest.cpp
    unittests/DOSGuard.cpp
    unittests/SubscriptionTest.cpp
    unittests/SubscriptionManagerTest.cpp
    unittests/util/TestObject.cpp
    unittests/util/StringUtils.cpp
    # ETL
    unittests/etl/ExtractionDataPipeTest.cpp
    unittests/etl/ExtractorTest.cpp
    unittests/etl/TransformerTest.cpp
    # RPC
    unittests/rpc/ErrorTests.cpp
    unittests/rpc/BaseTests.cpp
    unittests/rpc/RPCHelpersTest.cpp
    unittests/rpc/CountersTest.cpp
    unittests/rpc/AdminVerificationTest.cpp
    unittests/rpc/APIVersionTests.cpp
    unittests/rpc/ForwardingProxyTests.cpp
    ## RPC handlers
    unittests/rpc/handlers/DefaultProcessorTests.cpp
    unittests/rpc/handlers/TestHandlerTests.cpp
    unittests/rpc/handlers/AccountCurrenciesTest.cpp
    unittests/rpc/handlers/AccountLinesTest.cpp
    unittests/rpc/handlers/AccountTxTest.cpp
    unittests/rpc/handlers/AccountOffersTest.cpp
    unittests/rpc/handlers/AccountInfoTest.cpp
    unittests/rpc/handlers/AccountChannelsTest.cpp
    unittests/rpc/handlers/AccountNFTsTest.cpp
    unittests/rpc/handlers/BookOffersTest.cpp
    unittests/rpc/handlers/DepositAuthorizedTest.cpp
    unittests/rpc/handlers/GatewayBalancesTest.cpp
    unittests/rpc/handlers/TxTest.cpp
    unittests/rpc/handlers/TransactionEntryTest.cpp
    unittests/rpc/handlers/LedgerEntryTest.cpp
    unittests/rpc/handlers/LedgerRangeTest.cpp
    unittests/rpc/handlers/NoRippleCheckTest.cpp
    unittests/rpc/handlers/ServerInfoTest.cpp
    unittests/rpc/handlers/PingTest.cpp
    unittests/rpc/handlers/RandomTest.cpp
    unittests/rpc/handlers/NFTInfoTest.cpp
    unittests/rpc/handlers/NFTBuyOffersTest.cpp
    unittests/rpc/handlers/NFTSellOffersTest.cpp
    unittests/rpc/handlers/NFTHistoryTest.cpp
    unittests/rpc/handlers/SubscribeTest.cpp
    unittests/rpc/handlers/UnsubscribeTest.cpp
    unittests/rpc/handlers/LedgerDataTest.cpp
    unittests/rpc/handlers/AccountObjectsTest.cpp
    unittests/rpc/handlers/BookChangesTest.cpp
    unittests/rpc/handlers/LedgerTest.cpp
    # Backend
    unittests/backend/BackendFactoryTest.cpp
    unittests/backend/cassandra/BaseTests.cpp
    unittests/backend/cassandra/BackendTests.cpp
    unittests/backend/cassandra/RetryPolicyTests.cpp
    unittests/backend/cassandra/SettingsProviderTests.cpp
    unittests/backend/cassandra/ExecutionStrategyTests.cpp
    unittests/backend/cassandra/AsyncExecutorTests.cpp
    # Webserver
    unittests/webserver/ServerTest.cpp
    unittests/webserver/RPCServerHandlerTest.cpp)

  include(CMake/deps/gtest.cmake)

  # See https://github.com/google/googletest/issues/3475
  gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 10)

  # Fix for dwarf5 bug on ci 
  target_compile_options(clio PUBLIC -gdwarf-4)

  target_compile_definitions(${TEST_TARGET} PUBLIC UNITTEST_BUILD)
  target_include_directories(${TEST_TARGET} PRIVATE unittests)
  target_link_libraries(${TEST_TARGET} PUBLIC clio gtest::gtest)

  # Generate `clio_test-ccov` if coverage is enabled
  if(coverage)
    include(CMake/Coverage.cmake)
    add_coverage(${TEST_TARGET})
  endif()
endif()

include(CMake/install/install.cmake)
if(packaging)
    include(CMake/packaging.cmake) # TODO: file is actually missing?
endif()
