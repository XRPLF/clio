//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025 the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "etl/CacheLoaderInterface.hpp"
#include "etl/CacheUpdaterInterface.hpp"
#include "etl/ETLService.hpp"
#include "etl/ETLState.hpp"
#include "etl/ExtractorInterface.hpp"
#include "etl/InitialLoadObserverInterface.hpp"
#include "etl/LoadBalancerInterface.hpp"
#include "etl/LoaderInterface.hpp"
#include "etl/Models.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/MonitorProviderInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/SystemState.hpp"
#include "etl/TaskManagerInterface.hpp"
#include "etl/TaskManagerProviderInterface.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockLedgerPublisher.hpp"
#include "util/MockLoadBalancer.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/TestObject.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/async/context/SyncExecutionContext.hpp"
#include "util/async/impl/ErasedOperation.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/signals2/connection.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace util::config;

namespace {
constinit auto const kSEQ = 100;
constinit auto const kLEDGER_HASH =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

struct MockMonitor : public etl::MonitorInterface {
    MOCK_METHOD(void, notifySequenceLoaded, (uint32_t), (override));
    MOCK_METHOD(void, notifyWriteConflict, (uint32_t), (override));
    MOCK_METHOD(
        boost::signals2::scoped_connection,
        subscribeToNewSequence,
        (NewSequenceSignalType::slot_type const&),
        (override)
    );
    MOCK_METHOD(
        boost::signals2::scoped_connection,
        subscribeToDbStalled,
        (DbStalledSignalType::slot_type const&),
        (override)
    );
    MOCK_METHOD(void, run, (std::chrono::steady_clock::duration), (override));
    MOCK_METHOD(void, stop, (), (override));
};

struct MockExtractor : etl::ExtractorInterface {
    MOCK_METHOD(
        std::optional<etl::model::LedgerData>,
        extractLedgerWithDiff,
        (uint32_t),
        (override)
    );
    MOCK_METHOD(std::optional<etl::model::LedgerData>, extractLedgerOnly, (uint32_t), (override));
};

struct MockLoader : etl::LoaderInterface {
    using ExpectedType = std::expected<void, etl::LoaderError>;
    MOCK_METHOD(ExpectedType, load, (etl::model::LedgerData const&), (override));
    MOCK_METHOD(
        std::optional<ripple::LedgerHeader>,
        loadInitialLedger,
        (etl::model::LedgerData const&),
        (override)
    );
};

struct MockCacheLoader : etl::CacheLoaderInterface {
    MOCK_METHOD(void, load, (uint32_t), (override));
    MOCK_METHOD(void, stop, (), (noexcept, override));
    MOCK_METHOD(void, wait, (), (noexcept, override));
};

struct MockCacheUpdater : etl::CacheUpdaterInterface {
    MOCK_METHOD(void, update, (etl::model::LedgerData const&), (override));
    MOCK_METHOD(void, update, (uint32_t, std::vector<data::LedgerObject> const&), (override));
    MOCK_METHOD(void, update, (uint32_t, std::vector<etl::model::Object> const&), (override));
    MOCK_METHOD(void, setFull, (), (override));
};

struct MockInitialLoadObserver : etl::InitialLoadObserverInterface {
    MOCK_METHOD(
        void,
        onInitialLoadGotMoreObjects,
        (uint32_t, std::vector<etl::model::Object> const&, std::optional<std::string>),
        (override)
    );
};

struct MockTaskManager : etl::TaskManagerInterface {
    MOCK_METHOD(void, run, (std::size_t), (override));
    MOCK_METHOD(void, stop, (), (override));
};

struct MockTaskManagerProvider : etl::TaskManagerProviderInterface {
    MOCK_METHOD(
        std::unique_ptr<etl::TaskManagerInterface>,
        make,
        (util::async::AnyExecutionContext,
         std::reference_wrapper<etl::MonitorInterface>,
         uint32_t,
         std::optional<uint32_t>),
        (override)
    );
};

struct MockMonitorProvider : etl::MonitorProviderInterface {
    MOCK_METHOD(
        std::unique_ptr<etl::MonitorInterface>,
        make,
        (util::async::AnyExecutionContext,
         std::shared_ptr<BackendInterface>,
         std::shared_ptr<etl::NetworkValidatedLedgersInterface>,
         uint32_t,
         std::chrono::steady_clock::duration),
        (override)
    );
};

auto
createTestData(uint32_t seq)
{
    auto const header = createLedgerHeader(kLEDGER_HASH, seq);
    return etl::model::LedgerData{
        .transactions = {},
        .objects = {util::createObject(), util::createObject(), util::createObject()},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = seq,
    };
}
}  // namespace

struct ETLServiceTests : util::prometheus::WithPrometheus, MockBackendTest {
    using SameThreadTestContext = util::async::BasicExecutionContext<
        util::async::impl::SameThreadContext,
        util::async::impl::BasicStopSource,
        util::async::impl::SyncDispatchStrategy,
        util::async::impl::SystemContextProvider,
        util::async::impl::NoErrorHandler>;  // This will allow ASSERTs turned exceptions to
                                             // propagate

protected:
    SameThreadTestContext ctx_;
    util::config::ClioConfigDefinition config_{
        {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
        {"start_sequence",
         ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint32)},
        {"finish_sequence",
         ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint32)},
        {"extractor_threads", ConfigValue{ConfigType::Integer}.defaultValue(4)},
        {"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2)},
        {"cache.num_diffs", ConfigValue{ConfigType::Integer}.defaultValue(32)},
        {"cache.num_markers", ConfigValue{ConfigType::Integer}.defaultValue(48)},
        {"cache.num_cursors_from_diff", ConfigValue{ConfigType::Integer}.defaultValue(0)},
        {"cache.num_cursors_from_account", ConfigValue{ConfigType::Integer}.defaultValue(0)},
        {"cache.page_fetch_size", ConfigValue{ConfigType::Integer}.defaultValue(512)},
        {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async")}
    };
    MockSubscriptionManagerSharedPtr subscriptions_;
    std::shared_ptr<testing::NiceMock<MockLoadBalancer>> balancer_ =
        std::make_shared<testing::NiceMock<MockLoadBalancer>>();
    std::shared_ptr<testing::NiceMock<MockNetworkValidatedLedgers>> ledgers_ =
        std::make_shared<testing::NiceMock<MockNetworkValidatedLedgers>>();
    std::shared_ptr<testing::NiceMock<MockLedgerPublisher>> publisher_ =
        std::make_shared<testing::NiceMock<MockLedgerPublisher>>();
    std::shared_ptr<testing::NiceMock<MockCacheLoader>> cacheLoader_ =
        std::make_shared<testing::NiceMock<MockCacheLoader>>();
    std::shared_ptr<testing::NiceMock<MockCacheUpdater>> cacheUpdater_ =
        std::make_shared<testing::NiceMock<MockCacheUpdater>>();
    std::shared_ptr<testing::NiceMock<MockExtractor>> extractor_ =
        std::make_shared<testing::NiceMock<MockExtractor>>();
    std::shared_ptr<testing::NiceMock<MockLoader>> loader_ =
        std::make_shared<testing::NiceMock<MockLoader>>();
    std::shared_ptr<testing::NiceMock<MockInitialLoadObserver>> initialLoadObserver_ =
        std::make_shared<testing::NiceMock<MockInitialLoadObserver>>();
    std::shared_ptr<testing::NiceMock<MockTaskManagerProvider>> taskManagerProvider_ =
        std::make_shared<testing::NiceMock<MockTaskManagerProvider>>();
    std::shared_ptr<testing::NiceMock<MockMonitorProvider>> monitorProvider_ =
        std::make_shared<testing::NiceMock<MockMonitorProvider>>();
    std::shared_ptr<etl::SystemState> systemState_ = std::make_shared<etl::SystemState>();
    testing::StrictMock<testing::MockFunction<void(etl::SystemState::WriteCommand)>>
        mockWriteSignalCommandCallback_;
    boost::signals2::scoped_connection writeCommandConnection_{
        systemState_->writeCommandSignal.connect(mockWriteSignalCommandCallback_.AsStdFunction())
    };

    etl::ETLService service_{
        ctx_,
        config_,
        backend_,
        balancer_,
        ledgers_,
        publisher_,
        cacheLoader_,
        cacheUpdater_,
        extractor_,
        loader_,
        initialLoadObserver_,
        taskManagerProvider_,
        monitorProvider_,
        systemState_
    };
};

TEST_F(ETLServiceTests, GetInfoWithoutLastPublish)
{
    EXPECT_CALL(*balancer_, toJson())
        .WillOnce(testing::Return(boost::json::parse(R"JSON([{"test": "value"}])JSON")));
    EXPECT_CALL(*publisher_, getLastPublish())
        .WillOnce(testing::Return(std::chrono::system_clock::time_point{}));
    EXPECT_CALL(*publisher_, lastPublishAgeSeconds()).WillRepeatedly(testing::Return(0));

    auto result = service_.getInfo();
    auto expectedResult = boost::json::parse(R"JSON({
        "etl_sources": [{"test": "value"}],
        "is_writer": 0,
        "read_only": 0
    })JSON");

    EXPECT_TRUE(result == expectedResult);
    EXPECT_FALSE(result.contains("last_publish_age_seconds"));
}

TEST_F(ETLServiceTests, GetInfoWithLastPublish)
{
    EXPECT_CALL(*balancer_, toJson())
        .WillOnce(testing::Return(boost::json::parse(R"JSON([{"test": "value"}])JSON")));
    EXPECT_CALL(*publisher_, getLastPublish())
        .WillOnce(testing::Return(std::chrono::system_clock::now()));
    EXPECT_CALL(*publisher_, lastPublishAgeSeconds()).WillOnce(testing::Return(42));

    auto result = service_.getInfo();
    auto expectedResult = boost::json::parse(R"JSON({
        "etl_sources": [{"test": "value"}],
        "is_writer": 0,
        "read_only": 0,
        "last_publish_age_seconds": "42"
    })JSON");

    EXPECT_TRUE(result == expectedResult);
}

TEST_F(ETLServiceTests, IsAmendmentBlocked)
{
    EXPECT_FALSE(service_.isAmendmentBlocked());
}

TEST_F(ETLServiceTests, IsCorruptionDetected)
{
    EXPECT_FALSE(service_.isCorruptionDetected());
}

TEST_F(ETLServiceTests, GetETLState)
{
    EXPECT_CALL(*balancer_, getETLState()).WillOnce(testing::Return(etl::ETLState{}));

    auto result = service_.getETLState();
    EXPECT_TRUE(result.has_value());
}

TEST_F(ETLServiceTests, LastCloseAgeSeconds)
{
    EXPECT_CALL(*publisher_, lastCloseAgeSeconds()).WillOnce(testing::Return(10));

    auto result = service_.lastCloseAgeSeconds();
    EXPECT_GE(result, 0);
}

TEST_F(ETLServiceTests, RunWithEmptyDatabase)
{
    auto mockTaskManager = std::make_unique<testing::NiceMock<MockTaskManager>>();
    auto& mockTaskManagerRef = *mockTaskManager;
    auto ledgerData = createTestData(kSEQ);
    EXPECT_TRUE(systemState_->isLoadingCache);

    testing::Sequence const s;
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .InSequence(s)
        .WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillRepeatedly(testing::Return(kSEQ));
    EXPECT_CALL(*extractor_, extractLedgerOnly(kSEQ)).WillOnce(testing::Return(ledgerData));
    EXPECT_CALL(*balancer_, loadInitialLedger(kSEQ, testing::_, testing::_))
        .WillOnce(testing::Return(std::vector<std::string>{}));
    EXPECT_CALL(*loader_, loadInitialLedger).WillOnce(testing::Return(ripple::LedgerHeader{}));
    // In syncCacheWithDb()
    EXPECT_CALL(*backend_, hardFetchLedgerRange).Times(2).InSequence(s).WillRepeatedly([this]() {
        backend_->cache().update({}, kSEQ, false);
        return data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ};
    });
    EXPECT_CALL(mockTaskManagerRef, run);
    EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, kSEQ + 1, testing::_))
        .WillOnce([&](auto&&...) {
            EXPECT_FALSE(systemState_->isLoadingCache);
            return std::unique_ptr<etl::TaskManagerInterface>(mockTaskManager.release());
        });
    EXPECT_CALL(*monitorProvider_, make(testing::_, testing::_, testing::_, kSEQ + 1, testing::_))
        .WillOnce([this](auto, auto, auto, auto, auto) {
            EXPECT_TRUE(systemState_->isLoadingCache);
            return std::make_unique<testing::NiceMock<MockMonitor>>();
        });

    service_.run();
}

TEST_F(ETLServiceTests, RunWithPopulatedDatabase)
{
    EXPECT_TRUE(systemState_->isLoadingCache);
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*monitorProvider_, make(testing::_, testing::_, testing::_, kSEQ + 1, testing::_))
        .WillOnce([this](auto, auto, auto, auto, auto) {
            EXPECT_TRUE(systemState_->isLoadingCache);
            return std::make_unique<testing::NiceMock<MockMonitor>>();
        });
    EXPECT_CALL(*ledgers_, getMostRecent()).WillRepeatedly(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
}

TEST_F(ETLServiceTests, SyncCacheWithDbBeforeStartingMonitor)
{
    EXPECT_TRUE(systemState_->isLoadingCache);
    backend_->cache().update({}, kSEQ - 2, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));

    EXPECT_CALL(*backend_, fetchLedgerDiff(kSEQ - 1, testing::_));
    EXPECT_CALL(*cacheUpdater_, update(kSEQ - 1, std::vector<data::LedgerObject>()))
        .WillOnce([this](auto const seq, auto&&...) { backend_->cache().update({}, seq, false); });
    EXPECT_CALL(*backend_, fetchLedgerDiff(kSEQ, testing::_));
    EXPECT_CALL(*cacheUpdater_, update(kSEQ, std::vector<data::LedgerObject>()))
        .WillOnce([this](auto const seq, auto&&...) { backend_->cache().update({}, seq, false); });

    EXPECT_CALL(*monitorProvider_, make(testing::_, testing::_, testing::_, kSEQ + 1, testing::_))
        .WillOnce([this](auto, auto, auto, auto, auto) {
            EXPECT_TRUE(systemState_->isLoadingCache);
            return std::make_unique<testing::NiceMock<MockMonitor>>();
        });
    EXPECT_CALL(*ledgers_, getMostRecent()).WillRepeatedly(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
}

TEST_F(ETLServiceTests, WaitForValidatedLedgerIsAborted)
{
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).Times(2).WillRepeatedly(testing::Return(std::nullopt));

    // No other calls should happen because we exit early
    EXPECT_CALL(*extractor_, extractLedgerOnly).Times(0);
    EXPECT_CALL(*balancer_, loadInitialLedger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*loader_, loadInitialLedger).Times(0);
    EXPECT_CALL(*taskManagerProvider_, make).Times(0);

    service_.run();
}

TEST_F(ETLServiceTests, HandlesWriteConflictInMonitorSubscription)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void(uint32_t)> capturedCallback;

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });

    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence)
        .WillOnce([&capturedCallback](auto&& callback) {
            capturedCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled);
    EXPECT_CALL(mockMonitorRef, run);

    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .Times(2)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    writeCommandConnection_.disconnect();
    systemState_->writeCommandSignal(etl::SystemState::WriteCommand::StopWriting);

    EXPECT_CALL(*publisher_, publish(kSEQ + 1, testing::_, testing::_));
    ASSERT_TRUE(capturedCallback);
    capturedCallback(kSEQ + 1);

    EXPECT_FALSE(systemState_->isWriting);
}

TEST_F(ETLServiceTests, NormalFlowInMonitorSubscription)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void(uint32_t)> capturedCallback;

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });

    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence)
        .WillOnce([&capturedCallback](auto callback) {
            capturedCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled);
    EXPECT_CALL(mockMonitorRef, run);

    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .Times(2)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isWriting = false;
    std::vector<data::LedgerObject> const dummyDiff = {};

    EXPECT_CALL(*backend_, fetchLedgerDiff(kSEQ + 1, testing::_))
        .WillOnce(testing::Return(dummyDiff));
    EXPECT_CALL(
        *cacheUpdater_, update(kSEQ + 1, testing::A<std::vector<data::LedgerObject> const&>())
    );
    EXPECT_CALL(*publisher_, publish(kSEQ + 1, testing::_, testing::_));

    ASSERT_TRUE(capturedCallback);
    capturedCallback(kSEQ + 1);
}

TEST_F(ETLServiceTests, AttemptTakeoverWriter)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void()> capturedDbStalledCallback;

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });

    std::function<void(uint32_t)> onNewSeqCallback;
    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence).WillOnce([&onNewSeqCallback](auto cb) {
        onNewSeqCallback = std::move(cb);
        return boost::signals2::scoped_connection{};
    });
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled)
        .WillOnce([&capturedDbStalledCallback](auto callback) {
            capturedDbStalledCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, run);

    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isStrictReadonly = false;  // writer node
    systemState_->isWriting = false;         // but starts in readonly as usual

    auto mockTaskManager = std::make_unique<testing::NiceMock<MockTaskManager>>();
    auto& mockTaskManagerRef = *mockTaskManager;
    EXPECT_CALL(mockTaskManagerRef, run);

    EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, kSEQ + 1, testing::_))
        .WillOnce(testing::Return(std::move(mockTaskManager)));

    EXPECT_CALL(
        mockWriteSignalCommandCallback_, Call(etl::SystemState::WriteCommand::StartWriting)
    );

    ASSERT_TRUE(capturedDbStalledCallback);
    EXPECT_FALSE(
        systemState_->isWriting
    );  // will attempt to become writer after new sequence appears but not yet
    EXPECT_FALSE(systemState_->isWriterDecidingFallback);
    capturedDbStalledCallback();
    EXPECT_TRUE(systemState_->isWriting);                 // should attempt to become writer
    EXPECT_TRUE(systemState_->isWriterDecidingFallback);  // fallback mode activated
}

TEST_F(ETLServiceTests, GiveUpWriterAfterWriteConflict)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;

    std::function<void(uint32_t)> capturedCallback;

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });
    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence)
        .WillOnce([&capturedCallback](auto callback) {
            capturedCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled);
    EXPECT_CALL(mockMonitorRef, run);

    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .Times(2)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isWriting = true;
    writeCommandConnection_.disconnect();
    systemState_->writeCommandSignal(etl::SystemState::WriteCommand::StopWriting);

    EXPECT_CALL(*publisher_, publish(kSEQ + 1, testing::_, testing::_));

    ASSERT_TRUE(capturedCallback);
    capturedCallback(kSEQ + 1);

    EXPECT_FALSE(systemState_->isWriting);  // gives up writing
}

TEST_F(ETLServiceTests, CancelledLoadInitialLedger)
{
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillRepeatedly(testing::Return(kSEQ));
    EXPECT_CALL(*extractor_, extractLedgerOnly(kSEQ)).WillOnce(testing::Return(std::nullopt));

    // These calls should not happen because loading the initial ledger fails
    EXPECT_CALL(*balancer_, loadInitialLedger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*loader_, loadInitialLedger).Times(0);
    EXPECT_CALL(*taskManagerProvider_, make).Times(0);

    service_.run();
}

TEST_F(ETLServiceTests, WaitForValidatedLedgerIsAbortedLeadToFailToLoadInitialLedger)
{
    testing::Sequence const s;
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).InSequence(s).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).InSequence(s).WillOnce(testing::Return(kSEQ));

    // No other calls should happen because we exit early
    EXPECT_CALL(*extractor_, extractLedgerOnly).Times(0);
    EXPECT_CALL(*balancer_, loadInitialLedger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*loader_, loadInitialLedger).Times(0);
    EXPECT_CALL(*taskManagerProvider_, make).Times(0);

    service_.run();
}

TEST_F(ETLServiceTests, RunStopsIfInitialLoadIsCancelledByBalancer)
{
    constexpr uint32_t kMOCK_START_SEQUENCE = 123u;
    systemState_->isStrictReadonly = false;

    testing::Sequence const s;
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent)
        .InSequence(s)
        .WillOnce(testing::Return(kMOCK_START_SEQUENCE));
    EXPECT_CALL(*ledgers_, getMostRecent)
        .InSequence(s)
        .WillOnce(testing::Return(kMOCK_START_SEQUENCE + 10));

    auto const dummyLedgerData = createTestData(kMOCK_START_SEQUENCE);
    EXPECT_CALL(*extractor_, extractLedgerOnly(kMOCK_START_SEQUENCE))
        .WillOnce(testing::Return(dummyLedgerData));
    EXPECT_CALL(*balancer_, loadInitialLedger(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(std::unexpected{etl::InitialLedgerLoadError::Cancelled}));

    service_.run();

    EXPECT_TRUE(systemState_->isWriting);
    EXPECT_FALSE(service_.isAmendmentBlocked());
    EXPECT_FALSE(service_.isCorruptionDetected());
}

TEST_F(ETLServiceTests, DbStalledDoesNotTriggerSignalWhenStrictReadonly)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void()> capturedDbStalledCallback;

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });
    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence);
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled)
        .WillOnce([&capturedDbStalledCallback](auto callback) {
            capturedDbStalledCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, run);

    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isStrictReadonly = true;  // strict readonly mode
    systemState_->isWriting = false;

    // No signal should be emitted because node is in strict readonly mode
    // But fallback flag should still be set

    ASSERT_TRUE(capturedDbStalledCallback);
    EXPECT_FALSE(systemState_->isWriterDecidingFallback);
    capturedDbStalledCallback();
    EXPECT_TRUE(
        systemState_->isWriterDecidingFallback
    );  // fallback mode activated even in readonly
}

TEST_F(ETLServiceTests, DbStalledDoesNotTriggerSignalWhenAlreadyWriting)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void()> capturedDbStalledCallback;

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });
    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence);
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled)
        .WillOnce([&capturedDbStalledCallback](auto callback) {
            capturedDbStalledCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, run);

    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isStrictReadonly = false;
    systemState_->isWriting = true;  // already writing

    // No signal should be emitted because node is already writing
    // But fallback flag should still be set

    ASSERT_TRUE(capturedDbStalledCallback);
    EXPECT_FALSE(systemState_->isWriterDecidingFallback);
    capturedDbStalledCallback();
    EXPECT_TRUE(systemState_->isWriterDecidingFallback);  // fallback mode activated
}

TEST_F(ETLServiceTests, CacheUpdatesDependOnActualCacheState_WriterMode)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void(uint32_t)> capturedCallback;

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });
    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence)
        .WillOnce([&capturedCallback](auto callback) {
            capturedCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled);
    EXPECT_CALL(mockMonitorRef, run);

    // Set cache to be in sync with DB initially to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isWriting = true;  // In writer mode

    // Simulate cache is behind (e.g., update failed previously)
    // Cache latestLedgerSequence returns kSEQ (behind the new seq kSEQ + 1)
    std::vector<data::LedgerObject> const emptyObjs = {};
    backend_->cache().update(emptyObjs, kSEQ);  // Set cache to kSEQ

    std::vector<data::LedgerObject> const dummyDiff = {};
    EXPECT_CALL(*backend_, fetchLedgerDiff(kSEQ + 1, testing::_))
        .WillOnce(testing::Return(dummyDiff));

    // Cache should be updated even though we're in writer mode
    EXPECT_CALL(
        *cacheUpdater_, update(kSEQ + 1, testing::A<std::vector<data::LedgerObject> const&>())
    );

    EXPECT_CALL(*publisher_, publish(kSEQ + 1, testing::_, testing::_));

    ASSERT_TRUE(capturedCallback);
    capturedCallback(kSEQ + 1);
}

TEST_F(ETLServiceTests, OnlyCacheUpdatesWhenBackendIsCurrent)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void(uint32_t)> capturedCallback;
    // Set cache to be in sync with DB initially to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });
    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence)
        .WillOnce([&capturedCallback](auto callback) {
            capturedCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled);
    EXPECT_CALL(mockMonitorRef, run);

    // Set backend range to be at kSEQ + 1 (already current)
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillOnce(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}))
        .WillOnce(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}))
        .WillRepeatedly(
            testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ + 1})
        );
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isWriting = false;

    // Cache is behind (at kSEQ)
    std::vector<data::LedgerObject> const emptyObjs = {};
    backend_->cache().update(emptyObjs, kSEQ);

    std::vector<data::LedgerObject> const dummyDiff = {};
    EXPECT_CALL(*backend_, fetchLedgerDiff(kSEQ + 1, testing::_))
        .WillOnce(testing::Return(dummyDiff));
    EXPECT_CALL(
        *cacheUpdater_, update(kSEQ + 1, testing::A<std::vector<data::LedgerObject> const&>())
    );

    EXPECT_CALL(*publisher_, publish(kSEQ + 1, testing::_, testing::_));

    ASSERT_TRUE(capturedCallback);
    capturedCallback(kSEQ + 1);
}

TEST_F(ETLServiceTests, NoUpdatesWhenBothCacheAndBackendAreCurrent)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void(uint32_t)> capturedCallback;
    // Set cache to be in sync with DB initially to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });
    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence)
        .WillOnce([&capturedCallback](auto callback) {
            capturedCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled);
    EXPECT_CALL(mockMonitorRef, run);

    // Set backend range to be at kSEQ + 1 (already current)
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillOnce(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}))
        .WillOnce(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}))
        .WillRepeatedly(
            testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ + 1})
        );
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();

    // Cache is current (at kSEQ + 1)
    std::vector<data::LedgerObject> const emptyObjs = {};
    backend_->cache().update(emptyObjs, kSEQ + 1);

    // Neither should be updated
    EXPECT_CALL(*backend_, fetchLedgerDiff).Times(0);
    EXPECT_CALL(
        *cacheUpdater_, update(testing::_, testing::A<std::vector<data::LedgerObject> const&>())
    )
        .Times(0);

    EXPECT_CALL(*publisher_, publish(kSEQ + 1, testing::_, testing::_));

    ASSERT_TRUE(capturedCallback);
    capturedCallback(kSEQ + 1);
}

TEST_F(ETLServiceTests, StopWaitsForWriteCommandHandlersToComplete)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });

    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isStrictReadonly = false;

    auto mockTaskManager = std::make_unique<testing::NiceMock<MockTaskManager>>();

    EXPECT_CALL(
        mockWriteSignalCommandCallback_, Call(etl::SystemState::WriteCommand::StartWriting)
    );
    EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, kSEQ + 1, testing::_))
        .WillOnce(testing::Return(std::move(mockTaskManager)));

    // Emit a command
    systemState_->writeCommandSignal(etl::SystemState::WriteCommand::StartWriting);

    // The test context processes operations synchronously, so the handler should have run
    // Stop should wait for the handler to complete and disconnect the subscription
    service_.stop();

    // Verify stop() returned, meaning all handlers completed
    SUCCEED();
}

TEST_F(ETLServiceTests, WriteConflictIsHandledImmediately_NotDelayed)
{
    // This test verifies that write conflicts are handled immediately via signal,
    // not delayed until the next sequence notification (the old behavior)

    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();
    auto& mockMonitorRef = *mockMonitor;
    std::function<void(uint32_t)> capturedNewSeqCallback;

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });
    EXPECT_CALL(mockMonitorRef, subscribeToNewSequence)
        .WillOnce([&capturedNewSeqCallback](auto callback) {
            capturedNewSeqCallback = callback;
            return boost::signals2::scoped_connection{};
        });
    EXPECT_CALL(mockMonitorRef, subscribeToDbStalled);
    EXPECT_CALL(mockMonitorRef, run);

    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isWriting = true;

    // Emit StopWriting signal (simulating write conflict from Loader)
    EXPECT_CALL(mockWriteSignalCommandCallback_, Call(etl::SystemState::WriteCommand::StopWriting));
    systemState_->writeCommandSignal(etl::SystemState::WriteCommand::StopWriting);

    // The test context processes operations synchronously, so the handler should have run
    // immediately Verify that isWriting is immediately set to false
    EXPECT_FALSE(systemState_->isWriting);
}

TEST_F(ETLServiceTests, WriteCommandsAreSerializedOnStrand)
{
    auto mockMonitor = std::make_unique<testing::NiceMock<MockMonitor>>();

    EXPECT_CALL(*monitorProvider_, make).WillOnce([&mockMonitor](auto, auto, auto, auto, auto) {
        return std::move(mockMonitor);
    });

    // Set cache to be in sync with DB to avoid syncCacheWithDb loop
    backend_->cache().update({}, kSEQ, false);
    EXPECT_CALL(*backend_, hardFetchLedgerRange)
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillOnce(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));

    service_.run();
    systemState_->isStrictReadonly = false;
    systemState_->isWriting = false;

    auto mockTaskManager1 = std::make_unique<testing::NiceMock<MockTaskManager>>();
    auto mockTaskManager2 = std::make_unique<testing::NiceMock<MockTaskManager>>();

    // Set up expectations for the sequence of write commands
    // The signals should be processed in order: StartWriting, StopWriting, StartWriting
    {
        testing::InSequence const seq;

        // First StartWriting
        EXPECT_CALL(
            mockWriteSignalCommandCallback_, Call(etl::SystemState::WriteCommand::StartWriting)
        );
        EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, kSEQ + 1, testing::_))
            .WillOnce(testing::Return(std::move(mockTaskManager1)));

        // Then StopWriting
        EXPECT_CALL(
            mockWriteSignalCommandCallback_, Call(etl::SystemState::WriteCommand::StopWriting)
        );

        // Finally second StartWriting
        EXPECT_CALL(
            mockWriteSignalCommandCallback_, Call(etl::SystemState::WriteCommand::StartWriting)
        );
        EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, kSEQ + 1, testing::_))
            .WillOnce(testing::Return(std::move(mockTaskManager2)));
    }

    // Emit multiple signals rapidly - they should be serialized on the strand
    systemState_->writeCommandSignal(etl::SystemState::WriteCommand::StartWriting);
    systemState_->writeCommandSignal(etl::SystemState::WriteCommand::StopWriting);
    systemState_->writeCommandSignal(etl::SystemState::WriteCommand::StartWriting);

    // The test context processes operations synchronously, so all signals should have been
    // processed Final state should be writing (last signal was StartWriting)
    EXPECT_TRUE(systemState_->isWriting);
}
