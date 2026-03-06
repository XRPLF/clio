add_executable(clio_tests)

target_sources(
    clio_tests
    PRIVATE
        # Common
        app/CliArgsTests.cpp
        app/StopperTests.cpp
        app/VerifyConfigTests.cpp
        app/WebHandlersTests.cpp
        data/AmendmentCenterTests.cpp
        data/BackendCountersTests.cpp
        data/BackendInterfaceTests.cpp
        data/LedgerCacheTests.cpp
        data/LedgerCacheSaverTests.cpp
        data/cassandra/AsyncExecutorTests.cpp
        data/cassandra/ExecutionStrategyTests.cpp
        data/cassandra/LedgerHeaderCacheTests.cpp
        data/cassandra/RetryPolicyTests.cpp
        data/cassandra/SettingsProviderTests.cpp
        data/impl/InputFileTests.cpp
        data/impl/LedgerCacheFileTests.cpp
        data/impl/OutputFileTests.cpp
        # Cluster
        cluster/BackendTests.cpp
        cluster/ClioNodeTests.cpp
        cluster/ClusterCommunicationServiceTests.cpp
        cluster/MetricsTests.cpp
        cluster/RepeatedTaskTests.cpp
        cluster/WriterDeciderTests.cpp
        # ETL
        etl/AmendmentBlockHandlerTests.cpp
        etl/CacheLoaderSettingsTests.cpp
        etl/CacheLoaderTests.cpp
        etl/CursorFromAccountProviderTests.cpp
        etl/CursorFromDiffProviderTests.cpp
        etl/CursorFromFixDiffNumProviderTests.cpp
        etl/CorruptionDetectorTests.cpp
        etl/ETLStateTests.cpp
        etl/ETLServiceTests.cpp
        etl/ExtractionTests.cpp
        etl/ForwardingSourceTests.cpp
        etl/GrpcSourceTests.cpp
        etl/LedgerPublisherTests.cpp
        etl/LoadBalancerTests.cpp
        etl/LoadingTests.cpp
        etl/MonitorTests.cpp
        etl/NetworkValidatedLedgersTests.cpp
        etl/NFTHelpersTests.cpp
        etl/RegistryTests.cpp
        etl/SchedulingTests.cpp
        etl/SourceImplTests.cpp
        etl/SubscriptionSourceTests.cpp
        etl/SystemStateTests.cpp
        etl/TaskManagerTests.cpp
        etl/WriterStateTests.cpp
        etl/ext/CoreTests.cpp
        etl/ext/CacheTests.cpp
        etl/ext/MPTTests.cpp
        etl/ext/NFTTests.cpp
        etl/ext/SuccessorTests.cpp
        # Feed
        feed/BookChangesFeedTests.cpp
        feed/ForwardFeedTests.cpp
        feed/LedgerFeedTests.cpp
        feed/ProposedTransactionFeedTests.cpp
        feed/SingleFeedBaseTests.cpp
        feed/SubscriptionManagerTests.cpp
        feed/TrackableSignalTests.cpp
        feed/TransactionFeedTests.cpp
        # Logging
        util/log/LogServiceInitTests.cpp
        util/log/LoggerTests.cpp
        util/log/PrettyPathTests.cpp
        # Other
        JsonUtilTests.cpp
        Main.cpp
        Playground.cpp
        ProfilerTests.cpp
        # Migration
        migration/cassandra/FullTableScannerTests.cpp
        migration/cassandra/SpecTests.cpp
        migration/MigratorRegisterTests.cpp
        migration/MigratorStatusTests.cpp
        migration/MigrationInspectorFactoryTests.cpp
        migration/MigrationInspectorBaseTests.cpp
        migration/MigrationManagerBaseTests.cpp
        migration/MigrationManagerFactoryTests.cpp
        migration/SpecTests.cpp
        # RPC
        rpc/APIVersionTests.cpp
        rpc/BaseTests.cpp
        rpc/CountersTests.cpp
        rpc/ErrorTests.cpp
        rpc/ForwardingProxyTests.cpp
        rpc/common/CheckersTests.cpp
        rpc/common/SpecsTests.cpp
        rpc/common/TypesTests.cpp
        rpc/common/impl/HandlerProviderTests.cpp
        rpc/handlers/AccountChannelsTests.cpp
        rpc/handlers/AccountCurrenciesTests.cpp
        rpc/handlers/AccountInfoTests.cpp
        rpc/handlers/AccountLinesTests.cpp
        rpc/handlers/AccountMPTokenIssuancesTests.cpp
        rpc/handlers/AccountMPTokensTests.cpp
        rpc/handlers/AccountNFTsTests.cpp
        rpc/handlers/AccountObjectsTests.cpp
        rpc/handlers/AccountOffersTests.cpp
        rpc/handlers/AccountTxTests.cpp
        rpc/handlers/AMMInfoTests.cpp
        rpc/handlers/AllHandlerTests.cpp
        rpc/handlers/BookChangesTests.cpp
        rpc/handlers/BookOffersTests.cpp
        rpc/handlers/CredentialHelpersTests.cpp
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
        rpc/handlers/MPTHoldersTests.cpp
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
        rpc/handlers/VaultInfoTests.cpp
        rpc/JsonBoolTests.cpp
        rpc/RPCEngineTests.cpp
        rpc/RPCHelpersTests.cpp
        rpc/WorkQueueTests.cpp
        test_data/SslCert.cpp
        # Async framework
        util/async/AnyExecutionContextTests.cpp
        util/async/AnyOperationTests.cpp
        util/async/AnyStopTokenTests.cpp
        util/async/AnyStrandTests.cpp
        util/async/AsyncExecutionContextTests.cpp
        util/BatchingTests.cpp
        util/BlockingCacheTests.cpp
        util/ConceptsTests.cpp
        util/CoroutineGroupTests.cpp
        util/LedgerUtilsTests.cpp
        util/StringHashTests.cpp
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
        # Common utils
        util/AccountUtilsTests.cpp
        util/AssertTests.cpp
        util/BytesConverterTests.cpp
        util/ChannelTests.cpp
        util/CoroutineTest.cpp
        util/MoveTrackerTests.cpp
        util/ObservableValueTest.cpp
        util/ObservableValueAtomicTest.cpp
        util/RandomTests.cpp
        util/RepeatTests.cpp
        util/ResponseExpirationCacheTests.cpp
        util/RetryTests.cpp
        util/ScopeGuardTests.cpp
        util/SignalsHandlerTests.cpp
        util/ShasumTests.cpp
        util/SpawnTests.cpp
        util/StopHelperTests.cpp
        util/TimeUtilsTests.cpp
        util/TxUtilTests.cpp
        util/WithTimeout.cpp
        # Webserver
        web/AdminVerificationTests.cpp
        web/dosguard/DOSGuardTests.cpp
        web/dosguard/IntervalSweepHandlerTests.cpp
        web/dosguard/WeightsTests.cpp
        web/dosguard/WhitelistHandlerTests.cpp
        web/impl/ErrorHandlingTests.cpp
        web/ng/ResponseTests.cpp
        web/ng/RequestTests.cpp
        web/ng/RPCServerHandlerTests.cpp
        web/ng/ServerTests.cpp
        web/ng/SubscriptionContextTests.cpp
        web/ng/impl/ConnectionHandlerTests.cpp
        web/ng/impl/ErrorHandlingTests.cpp
        web/ng/impl/HttpConnectionTests.cpp
        web/ng/impl/ServerSslContextTests.cpp
        web/ng/impl/WsConnectionTests.cpp
        web/ProxyIpResolverTests.cpp
        web/RPCServerHandlerTests.cpp
        web/ServerTests.cpp
        web/SubscriptionContextTests.cpp
        # Config
        util/config/ArrayTests.cpp
        util/config/ArrayViewTests.cpp
        util/config/ClioConfigDefinitionTests.cpp
        util/config/ConfigValueTests.cpp
        util/config/ObjectViewTests.cpp
        util/config/ConfigFileJsonTests.cpp
        util/config/ValueViewTests.cpp
)

# See https://github.com/google/googletest/issues/3475
gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 90)

target_include_directories(clio_tests PRIVATE .)
target_link_libraries(
    clio_tests
    PUBLIC clio_testing_common
    PRIVATE spdlog::spdlog
)
set_target_properties(
    clio_tests
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Generate `coverage_report` target if coverage is enabled
if(coverage)
    if(DEFINED CODE_COVERAGE_REPORT_FORMAT)
        set(CODE_COVERAGE_FORMAT ${CODE_COVERAGE_REPORT_FORMAT})
    else()
        set(CODE_COVERAGE_FORMAT html-details)
    endif()

    if(DEFINED CODE_COVERAGE_TESTS_ARGS)
        set(TESTS_ADDITIONAL_ARGS ${CODE_COVERAGE_TESTS_ARGS})
        separate_arguments(TESTS_ADDITIONAL_ARGS)
    else()
        set(TESTS_ADDITIONAL_ARGS "")
    endif()

    set(GCOVR_ADDITIONAL_ARGS --exclude-throw-branches -s)

    setup_target_for_coverage_gcovr(
        NAME coverage_report
        FORMAT ${CODE_COVERAGE_FORMAT}
        EXECUTABLE clio_tests
        EXECUTABLE_ARGS --gtest_brief=1 ${TESTS_ADDITIONAL_ARGS}
        EXCLUDE
            "tests"
            "src/data/cassandra"
            "src/data/CassandraBackend.hpp"
            "src/data/BackendFactory.*"
        DEPENDENCIES clio_tests
    )
endif()
