cmake_minimum_required(VERSION 3.16.3)

project(clio)

# Only GCC is supported.
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
  message(FATAL_ERROR "GCC 11+ required for building clio")
endif()

option(verbose "Verbose build" TRUE)
if(verbose)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
endif()

# REVIEW: Who is setting this variable?
if(PACKAGING)
  add_definitions(-DPKG=1)
endif()

#c++20 removed std::result_of but boost 1.75 is still using it.
add_definitions(-DBOOST_ASIO_HAS_STD_INVOKE_RESULT=1)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.82 REQUIRED
  COMPONENTS
    program_options
    coroutine
    system
    log
    log_setup
)

find_package(OpenSSL 1.1.1 REQUIRED)
find_package(cassandra-cpp-driver REQUIRED)
find_package(fmt REQUIRED)
find_package(xrpl REQUIRED)

set_target_properties(OpenSSL::SSL PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS OPENSSL_NO_SSL2
)

add_library(clio)
target_compile_features(clio PUBLIC cxx_std_20)
# TODO: Include directory will be wrong when installed.
target_include_directories(clio PUBLIC src)
target_compile_definitions(clio PUBLIC 
  BOOST_ASIO_DISABLE_HANDLER_TYPE_REQUIREMENTS 
  BOOST_ASIO_HAS_STD_INVOKE_RESULT 
  BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT
  BOOST_BEAST_ALLOW_DEPRECATED
  BOOST_CONTAINER_FWD_BAD_DEQUE
  BOOST_COROUTINES_NO_DEPRECATION_WARNING)
target_compile_options(clio PUBLIC
  -Wall
  -Werror
  -Wno-narrowing
  -Wno-deprecated-declarations 
  -Wno-dangling-else
  -Wno-unused-but-set-variable)
target_link_libraries(clio PUBLIC
  Boost::boost
  Boost::coroutine
  Boost::program_options
  Boost::system
  Boost::log
  Boost::log_setup
  cassandra-cpp-driver::cassandra-cpp-driver
  fmt::fmt
  OpenSSL::Crypto
  OpenSSL::SSL
  xrpl::libxrpl
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
target_link_libraries(clio INTERFACE Threads::Threads)

include(CMake/ClioVersion.cmake)
include(CMake/deps/SourceLocation.cmake)

target_sources(clio PRIVATE
  ## Main
  src/main/impl/Build.cpp
  ## Backend
  src/backend/BackendInterface.cpp
  src/backend/LedgerCache.cpp
  ## NextGen Backend
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
  # RPC impl
  src/rpc/common/impl/HandlerProvider.cpp
  ## RPC handler
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

add_executable(clio_server src/main/main.cpp)
target_link_libraries(clio_server PUBLIC clio)

option(tests "Build tests" FALSE)
if(tests)
  set(TEST_TARGET clio_tests)
  add_executable(${TEST_TARGET}
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
    unittests/webserver/ServerTest.cpp
    unittests/webserver/RPCServerHandlerTest.cpp)

  include(CMake/deps/gtest.cmake)

  # fix for dwarf5 bug on ci 
  target_compile_options(clio PUBLIC -gdwarf-4)

  target_compile_definitions(${TEST_TARGET} PUBLIC UNITTEST_BUILD)
  target_include_directories(${TEST_TARGET} PRIVATE unittests)
  # REVIEW: No link to clio library?

  # if CODE_COVERAGE enable, add clio_test-ccov
  if(CODE_COVERAGE)
    include(CMake/coverage.cmake)
    # REVIEW: Misspelled "coverage"?
    add_converage(${TEST_TARGET})
  endif()
endif()

include(CMake/install/install.cmake)
if(PACKAGING)
    include(CMake/packaging.cmake)
endif()
