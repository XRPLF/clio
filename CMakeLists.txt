cmake_minimum_required(VERSION 3.16.3)
project(clio)

# ========================================================================== #
#                                   Options                                  #
# ========================================================================== #
option (verbose   "Verbose build"                                     FALSE)
option (tests     "Build tests"                                       FALSE)
option (benchmark "Build benchmarks"                                  FALSE)
option (docs      "Generate doxygen docs"                             FALSE)
option (coverage  "Build test coverage report"                        FALSE)
option (packaging "Create distribution packages"                      FALSE)
option (lint      "Run clang-tidy checks during compilation"          FALSE)
# ========================================================================== #
set (san "" CACHE STRING "Add sanitizer instrumentation")
set (CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set_property (CACHE san PROPERTY STRINGS ";undefined;memory;address;thread")
# ========================================================================== #

# Include required modules
include (CMake/Ccache.cmake)
include (CheckCXXCompilerFlag)
include (CMake/ClangTidy.cmake)

# Set coverage build options
if (tests AND coverage)
  include (CMake/CodeCoverage.cmake)
  append_coverage_compiler_flags()
endif()

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
include (CMake/deps/libbacktrace.cmake)

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
  PUBLIC Boost::stacktrace_backtrace
  PUBLIC cassandra-cpp-driver::cassandra-cpp-driver
  PUBLIC fmt::fmt
  PUBLIC OpenSSL::Crypto
  PUBLIC OpenSSL::SSL
  PUBLIC xrpl::libxrpl
  PUBLIC dl
  PUBLIC libbacktrace::libbacktrace

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
  src/data/BackendCounters.cpp
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
  src/etl/ETLState.cpp
  src/etl/LoadBalancer.cpp
  src/etl/impl/ForwardCache.cpp
  ## Feed
  src/feed/SubscriptionManager.cpp
  src/feed/impl/TransactionFeed.cpp
  src/feed/impl/LedgerFeed.cpp
  src/feed/impl/ProposedTransactionFeed.cpp
  src/feed/impl/SingleFeedBase.cpp
  ## Web
  src/web/impl/AdminVerificationStrategy.cpp
  src/web/IntervalSweepHandler.cpp
  src/web/Resolver.cpp
  ## RPC
  src/rpc/Errors.cpp
  src/rpc/Factories.cpp
  src/rpc/AMMHelpers.cpp
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
  src/rpc/handlers/AMMInfo.cpp
  src/rpc/handlers/BookChanges.cpp
  src/rpc/handlers/BookOffers.cpp
  src/rpc/handlers/DepositAuthorized.cpp
  src/rpc/handlers/GatewayBalances.cpp
  src/rpc/handlers/Ledger.cpp
  src/rpc/handlers/LedgerData.cpp
  src/rpc/handlers/LedgerEntry.cpp
  src/rpc/handlers/LedgerRange.cpp
  src/rpc/handlers/NFTsByIssuer.cpp
  src/rpc/handlers/NFTBuyOffers.cpp
  src/rpc/handlers/NFTHistory.cpp
  src/rpc/handlers/NFTInfo.cpp
  src/rpc/handlers/NFTOffersCommon.cpp
  src/rpc/handlers/NFTSellOffers.cpp
  src/rpc/handlers/NoRippleCheck.cpp
  src/rpc/handlers/Random.cpp
  src/rpc/handlers/TransactionEntry.cpp
  ## Util
  src/util/config/Config.cpp
  src/util/log/Logger.cpp
  src/util/prometheus/Http.cpp
  src/util/prometheus/Label.cpp
  src/util/prometheus/MetricBase.cpp
  src/util/prometheus/MetricBuilder.cpp
  src/util/prometheus/MetricsFamily.cpp
  src/util/prometheus/OStream.cpp
  src/util/prometheus/Prometheus.cpp
  src/util/Random.cpp
  src/util/requests/RequestBuilder.cpp
  src/util/requests/Types.cpp
  src/util/requests/WsConnection.cpp
  src/util/requests/impl/SslContext.cpp
  src/util/Taggable.cpp
  src/util/TerminationHandler.cpp
  src/util/TxUtils.cpp
  src/util/LedgerUtils.cpp
)

# Clio server
add_executable (clio_server src/main/Main.cpp)
target_link_libraries (clio_server PRIVATE clio)
target_link_options(clio_server
  PRIVATE
    $<$<AND:$<NOT:$<BOOL:${APPLE}>>,$<NOT:$<BOOL:${san}>>>:-static-libstdc++ -static-libgcc>
)

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
    unittests/util/TestGlobals.cpp
    unittests/util/AssertTests.cpp
    unittests/util/BatchingTests.cpp
    unittests/util/TestHttpServer.cpp
    unittests/util/TestObject.cpp
    unittests/util/TestWsServer.cpp
    unittests/util/TxUtilTests.cpp
    unittests/util/StringUtils.cpp
    unittests/util/LedgerUtilsTests.cpp
    ## Prometheus support
    unittests/util/prometheus/CounterTests.cpp
    unittests/util/prometheus/GaugeTests.cpp
    unittests/util/prometheus/HistogramTests.cpp
    unittests/util/prometheus/HttpTests.cpp
    unittests/util/prometheus/LabelTests.cpp
    unittests/util/prometheus/MetricBuilderTests.cpp
    unittests/util/prometheus/MetricsFamilyTests.cpp
    unittests/util/prometheus/OStreamTests.cpp
    ## Async framework
    unittests/util/async/AnyExecutionContextTests.cpp
    unittests/util/async/AnyStrandTests.cpp
    unittests/util/async/AnyOperationTests.cpp
    unittests/util/async/AnyStopTokenTests.cpp
    unittests/util/async/AsyncExecutionContextTests.cpp
    ## Requests framework
    unittests/util/requests/RequestBuilderTests.cpp
    unittests/util/requests/SslContextTests.cpp
    unittests/util/requests/WsConnectionTests.cpp
    # ETL
    unittests/etl/ExtractionDataPipeTests.cpp
    unittests/etl/ExtractorTests.cpp
    unittests/etl/TransformerTests.cpp
    unittests/etl/CacheLoaderTests.cpp
    unittests/etl/AmendmentBlockHandlerTests.cpp
    unittests/etl/LedgerPublisherTests.cpp
    unittests/etl/ETLStateTests.cpp
    # RPC
    unittests/rpc/ErrorTests.cpp
    unittests/rpc/BaseTests.cpp
    unittests/rpc/RPCHelpersTests.cpp
    unittests/rpc/CountersTests.cpp
    unittests/rpc/APIVersionTests.cpp
    unittests/rpc/ForwardingProxyTests.cpp
    unittests/rpc/WorkQueueTests.cpp
    unittests/rpc/AmendmentsTests.cpp
    unittests/rpc/JsonBoolTests.cpp
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
    unittests/rpc/handlers/NFTsByIssuerTest.cpp
    unittests/rpc/handlers/NFTSellOffersTests.cpp
    unittests/rpc/handlers/NFTHistoryTests.cpp
    unittests/rpc/handlers/SubscribeTests.cpp
    unittests/rpc/handlers/UnsubscribeTests.cpp
    unittests/rpc/handlers/LedgerDataTests.cpp
    unittests/rpc/handlers/AccountObjectsTests.cpp
    unittests/rpc/handlers/BookChangesTests.cpp
    unittests/rpc/handlers/LedgerTests.cpp
    unittests/rpc/handlers/VersionHandlerTests.cpp
    unittests/rpc/handlers/AMMInfoTests.cpp
    # Backend
    unittests/data/BackendFactoryTests.cpp
    unittests/data/BackendCountersTests.cpp
    unittests/data/cassandra/BaseTests.cpp
    unittests/data/cassandra/BackendTests.cpp
    unittests/data/cassandra/RetryPolicyTests.cpp
    unittests/data/cassandra/SettingsProviderTests.cpp
    unittests/data/cassandra/ExecutionStrategyTests.cpp
    unittests/data/cassandra/AsyncExecutorTests.cpp
    # Webserver
    unittests/web/AdminVerificationTests.cpp
    unittests/web/ServerTests.cpp
    unittests/web/RPCServerHandlerTests.cpp
    unittests/web/WhitelistHandlerTests.cpp
    unittests/web/SweepHandlerTests.cpp
    # Feed
    unittests/feed/SubscriptionManagerTests.cpp
    unittests/feed/SingleFeedBaseTests.cpp
    unittests/feed/ProposedTransactionFeedTests.cpp
    unittests/feed/BookChangesFeedTests.cpp
    unittests/feed/LedgerFeedTests.cpp
    unittests/feed/TransactionFeedTests.cpp
    unittests/feed/ForwardFeedTests.cpp
    unittests/feed/TrackableSignalTests.cpp)

  include (CMake/deps/gtest.cmake)

  # See https://github.com/google/googletest/issues/3475
  gtest_discover_tests (clio_tests DISCOVERY_TIMEOUT 10)

  # Fix for dwarf5 bug on ci
  target_compile_options (clio PUBLIC -gdwarf-4)

  target_compile_definitions (${TEST_TARGET} PUBLIC UNITTEST_BUILD)
  target_include_directories (${TEST_TARGET} PRIVATE unittests)
  target_link_libraries (${TEST_TARGET} PUBLIC clio gtest::gtest)

  # Generate `coverage_report` target if coverage is enabled
  if (coverage)
    if (DEFINED CODE_COVERAGE_REPORT_FORMAT)
      set(CODE_COVERAGE_FORMAT ${CODE_COVERAGE_REPORT_FORMAT})
    else()
      set(CODE_COVERAGE_FORMAT html-details)
    endif()

    if (DEFINED CODE_COVERAGE_TESTS_ARGS)
      set(TESTS_ADDITIONAL_ARGS ${CODE_COVERAGE_TESTS_ARGS})
      separate_arguments(TESTS_ADDITIONAL_ARGS)
    else()
      set(TESTS_ADDITIONAL_ARGS "")
    endif()

    set (GCOVR_ADDITIONAL_ARGS --exclude-throw-branches -s)

    setup_target_for_coverage_gcovr(
        NAME coverage_report
        FORMAT ${CODE_COVERAGE_FORMAT}
        EXECUTABLE clio_tests
        EXECUTABLE_ARGS --gtest_brief=1 ${TESTS_ADDITIONAL_ARGS}
        EXCLUDE "unittests"
        DEPENDENCIES clio_tests
    )
  endif ()
endif ()

# Benchmarks
if (benchmark)
  set (BENCH_TARGET clio_benchmarks)
  add_executable (${BENCH_TARGET}
    # Common
    benchmarks/Main.cpp
    benchmarks/Playground.cpp
    # ExecutionContext
    benchmarks/util/async/ExecutionContextBenchmarks.cpp
  )

  include (CMake/deps/gbench.cmake)

  target_include_directories (${BENCH_TARGET} PRIVATE benchmarks)
  target_link_libraries (${BENCH_TARGET} PUBLIC clio benchmark::benchmark_main)
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
