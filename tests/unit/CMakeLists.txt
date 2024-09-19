add_executable(clio_tests)

target_sources(
  clio_tests
  PRIVATE # Common
          ConfigTests.cpp
          app/CliArgsTests.cpp
          data/AmendmentCenterTests.cpp
          data/BackendCountersTests.cpp
          data/BackendInterfaceTests.cpp
          data/cassandra/AsyncExecutorTests.cpp
          data/cassandra/ExecutionStrategyTests.cpp
          data/cassandra/RetryPolicyTests.cpp
          data/cassandra/SettingsProviderTests.cpp
          # ETL
          etl/AmendmentBlockHandlerTests.cpp
          etl/CacheLoaderSettingsTests.cpp
          etl/CacheLoaderTests.cpp
          etl/CursorFromAccountProviderTests.cpp
          etl/CursorFromDiffProviderTests.cpp
          etl/CursorFromFixDiffNumProviderTests.cpp
          etl/CorruptionDetectorTests.cpp
          etl/ETLStateTests.cpp
          etl/ExtractionDataPipeTests.cpp
          etl/ExtractorTests.cpp
          etl/ForwardingCacheTests.cpp
          etl/ForwardingSourceTests.cpp
          etl/GrpcSourceTests.cpp
          etl/LedgerPublisherTests.cpp
          etl/LoadBalancerTests.cpp
          etl/NFTHelpersTests.cpp
          etl/SourceImplTests.cpp
          etl/SubscriptionSourceTests.cpp
          etl/TransformerTests.cpp
          # Feed
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
          # RPC
          rpc/APIVersionTests.cpp
          rpc/BaseTests.cpp
          rpc/CountersTests.cpp
          rpc/ErrorTests.cpp
          rpc/ForwardingProxyTests.cpp
          rpc/common/CheckersTests.cpp
          rpc/common/SpecsTests.cpp
          rpc/common/TypesTests.cpp
          rpc/handlers/AccountChannelsTests.cpp
          rpc/handlers/AccountCurrenciesTests.cpp
          rpc/handlers/AccountInfoTests.cpp
          rpc/handlers/AccountLinesTests.cpp
          rpc/handlers/AccountNFTsTests.cpp
          rpc/handlers/AccountObjectsTests.cpp
          rpc/handlers/AccountOffersTests.cpp
          rpc/handlers/AccountTxTests.cpp
          rpc/handlers/AMMInfoTests.cpp
          rpc/handlers/BookChangesTests.cpp
          rpc/handlers/BookOffersTests.cpp
          rpc/handlers/DefaultProcessorTests.cpp
          rpc/handlers/DepositAuthorizedTests.cpp
          rpc/handlers/FeatureTests.cpp
          rpc/handlers/GatewayBalancesTests.cpp
          rpc/handlers/GetAggregatePriceTests.cpp
          rpc/handlers/LedgerDataTests.cpp
          rpc/handlers/LedgerEntryTests.cpp
          rpc/handlers/LedgerIndexTests.cpp
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
          rpc/RPCHelpersTests.cpp
          rpc/WorkQueueTests.cpp
          util/AccountUtilsTests.cpp
          util/AssertTests.cpp
          # Async framework
          util/async/AnyExecutionContextTests.cpp
          util/async/AnyOperationTests.cpp
          util/async/AnyStopTokenTests.cpp
          util/async/AnyStrandTests.cpp
          util/async/AsyncExecutionContextTests.cpp
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
          # Requests framework
          util/requests/RequestBuilderTests.cpp
          util/requests/SslContextTests.cpp
          util/requests/WsConnectionTests.cpp
          util/RandomTests.cpp
          util/RetryTests.cpp
          util/RepeatTests.cpp
          util/SignalsHandlerTests.cpp
          util/TimeUtilsTests.cpp
          util/TxUtilTests.cpp
          # Webserver
          web/AdminVerificationTests.cpp
          web/dosguard/DOSGuardTests.cpp
          web/dosguard/IntervalSweepHandlerTests.cpp
          web/dosguard/WhitelistHandlerTests.cpp
          web/impl/ServerSslContextTests.cpp
          web/RPCServerHandlerTests.cpp
          web/ServerTests.cpp
          # New Config
          util/newconfig/ArrayTests.cpp
          util/newconfig/ArrayViewTests.cpp
          util/newconfig/ClioConfigDefinitionTests.cpp
          util/newconfig/ConfigValueTests.cpp
          util/newconfig/ObjectViewTests.cpp
          util/newconfig/JsonConfigFileTests.cpp
          util/newconfig/ValueViewTests.cpp
)

configure_file(test_data/cert.pem ${CMAKE_BINARY_DIR}/tests/unit/test_data/cert.pem COPYONLY)
target_compile_definitions(clio_tests PRIVATE TEST_DATA_SSL_CERT_PATH="tests/unit/test_data/cert.pem")

configure_file(test_data/key.pem ${CMAKE_BINARY_DIR}/tests/unit/test_data/key.pem COPYONLY)
target_compile_definitions(clio_tests PRIVATE TEST_DATA_SSL_KEY_PATH="tests/unit/test_data/key.pem")

# See https://github.com/google/googletest/issues/3475
gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 90)

# Fix for dwarf5 bug on ci
target_compile_options(clio_options INTERFACE -gdwarf-4)

target_compile_definitions(clio_tests PUBLIC UNITTEST_BUILD)
target_include_directories(clio_tests PRIVATE .)
target_link_libraries(clio_tests PUBLIC clio_testing_common)
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
    "tests"
    "src/data/cassandra"
    "src/data/CassandraBackend.hpp"
    "src/data/BackendFactory.*"
    DEPENDENCIES
    clio_tests
  )
endif ()
