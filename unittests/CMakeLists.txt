add_executable(clio_tests)

target_sources(
  clio_tests
  PRIVATE # Common
          ConfigTests.cpp
          data/BackendCountersTests.cpp
          data/BackendFactoryTests.cpp
          data/BackendInterfaceTests.cpp
          data/cassandra/AsyncExecutorTests.cpp
          # Webserver
          data/cassandra/BackendTests.cpp
          data/cassandra/BaseTests.cpp
          data/cassandra/ExecutionStrategyTests.cpp
          data/cassandra/RetryPolicyTests.cpp
          data/cassandra/SettingsProviderTests.cpp
          DOSGuardTests.cpp
          etl/AmendmentBlockHandlerTests.cpp
          etl/CacheLoaderSettingsTests.cpp
          etl/CacheLoaderTests.cpp
          etl/CursorFromAccountProviderTests.cpp
          etl/CursorFromDiffProviderTests.cpp
          etl/CursorFromFixDiffNumProviderTests.cpp
          etl/ETLStateTests.cpp
          etl/ExtractionDataPipeTests.cpp
          etl/ExtractorTests.cpp
          etl/ForwardingCacheTests.cpp
          etl/ForwardingSourceTests.cpp
          etl/GrpcSourceTests.cpp
          etl/LedgerPublisherTests.cpp
          etl/SourceTests.cpp
          etl/SubscriptionSourceDependenciesTests.cpp
          etl/SubscriptionSourceTests.cpp
          etl/TransformerTests.cpp
          # RPC
          feed/BookChangesFeedTests.cpp
          feed/ForwardFeedTests.cpp
          feed/LedgerFeedTests.cpp
          feed/ProposedTransactionFeedTests.cpp
          feed/SingleFeedBaseTests.cpp
          feed/SubscriptionManagerTests.cpp
          feed/TrackableSignalTests.cpp
          feed/TransactionFeedTests.cpp
          JsonUtilTests.cpp
          LoggerTests.cpp
          Main.cpp
          Playground.cpp
          ProfilerTests.cpp
          rpc/AmendmentsTests.cpp
          rpc/APIVersionTests.cpp
          rpc/BaseTests.cpp
          rpc/CountersTests.cpp
          rpc/ErrorTests.cpp
          rpc/ForwardingProxyTests.cpp
          rpc/handlers/AccountChannelsTests.cpp
          rpc/handlers/AccountCurrenciesTests.cpp
          rpc/handlers/AccountInfoTests.cpp
          rpc/handlers/AccountLinesTests.cpp
          rpc/handlers/AccountNFTsTests.cpp
          rpc/handlers/AccountObjectsTests.cpp
          rpc/handlers/AccountOffersTests.cpp
          rpc/handlers/AccountTxTests.cpp
          rpc/handlers/AMMInfoTests.cpp
          # Backend
          rpc/handlers/BookChangesTests.cpp
          rpc/handlers/BookOffersTests.cpp
          rpc/handlers/DefaultProcessorTests.cpp
          rpc/handlers/DepositAuthorizedTests.cpp
          rpc/handlers/GatewayBalancesTests.cpp
          rpc/handlers/LedgerDataTests.cpp
          rpc/handlers/LedgerEntryTests.cpp
          rpc/handlers/LedgerRangeTests.cpp
          rpc/handlers/LedgerTests.cpp
          rpc/handlers/NFTBuyOffersTests.cpp
          rpc/handlers/NFTHistoryTests.cpp
          rpc/handlers/NFTInfoTests.cpp
          rpc/handlers/NFTsByIssuerTest.cpp
          rpc/handlers/NFTSellOffersTests.cpp
          rpc/handlers/NoRippleCheckTests.cpp
          rpc/handlers/PingTests.cpp
          rpc/handlers/RandomTests.cpp
          rpc/handlers/ServerInfoTests.cpp
          rpc/handlers/SubscribeTests.cpp
          rpc/handlers/TestHandlerTests.cpp
          rpc/handlers/TransactionEntryTests.cpp
          rpc/handlers/TxTests.cpp
          rpc/handlers/UnsubscribeTests.cpp
          rpc/handlers/VersionHandlerTests.cpp
          rpc/JsonBoolTests.cpp
          # RPC handlers
          rpc/RPCHelpersTests.cpp
          rpc/WorkQueueTests.cpp
          util/AssertTests.cpp
          util/async/AnyExecutionContextTests.cpp
          util/async/AnyOperationTests.cpp
          util/async/AnyStopTokenTests.cpp
          util/async/AnyStrandTests.cpp
          util/async/AsyncExecutionContextTests.cpp
          # Requests framework
          util/BatchingTests.cpp
          util/LedgerUtilsTests.cpp
          # Prometheus support
          util/prometheus/BoolTests.cpp
          util/prometheus/CounterTests.cpp
          util/prometheus/GaugeTests.cpp
          util/prometheus/HistogramTests.cpp
          util/prometheus/HttpTests.cpp
          util/prometheus/LabelTests.cpp
          util/prometheus/MetricBuilderTests.cpp
          util/prometheus/MetricsFamilyTests.cpp
          util/prometheus/OStreamTests.cpp
          util/requests/RequestBuilderTests.cpp
          util/requests/SslContextTests.cpp
          util/requests/WsConnectionTests.cpp
          # ETL
          util/RetryTests.cpp
          # Async framework
          util/StringUtils.cpp
          util/TestGlobals.cpp
          util/TestHttpServer.cpp
          util/TestObject.cpp
          util/TestWsServer.cpp
          util/TxUtilTests.cpp
          web/AdminVerificationTests.cpp
          web/RPCServerHandlerTests.cpp
          web/ServerTests.cpp
          web/SweepHandlerTests.cpp
          # Feed
          web/WhitelistHandlerTests.cpp
)

include(deps/gtest)

# See https://github.com/google/googletest/issues/3475
gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 90)

# Fix for dwarf5 bug on ci
target_compile_options(clio_options INTERFACE -gdwarf-4)

target_compile_definitions(clio_tests PUBLIC UNITTEST_BUILD)
target_include_directories(clio_tests PRIVATE .)
target_link_libraries(clio_tests PUBLIC clio gtest::gtest)
set_target_properties(clio_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Generate `coverage_report` target if coverage is enabled
if (coverage)
  if (DEFINED CODE_COVERAGE_REPORT_FORMAT)
    set(CODE_COVERAGE_FORMAT ${CODE_COVERAGE_REPORT_FORMAT})
  else ()
    set(CODE_COVERAGE_FORMAT html-details)
  endif ()

  if (DEFINED CODE_COVERAGE_TESTS_ARGS)
    set(TESTS_ADDITIONAL_ARGS ${CODE_COVERAGE_TESTS_ARGS})
    separate_arguments(TESTS_ADDITIONAL_ARGS)
  else ()
    set(TESTS_ADDITIONAL_ARGS "")
  endif ()

  set(GCOVR_ADDITIONAL_ARGS --exclude-throw-branches -s)

  setup_target_for_coverage_gcovr(
    NAME
    coverage_report
    FORMAT
    ${CODE_COVERAGE_FORMAT}
    EXECUTABLE
    clio_tests
    EXECUTABLE_ARGS
    --gtest_brief=1
    ${TESTS_ADDITIONAL_ARGS}
    EXCLUDE
    "unittests"
    DEPENDENCIES
    clio_tests
  )
endif ()
