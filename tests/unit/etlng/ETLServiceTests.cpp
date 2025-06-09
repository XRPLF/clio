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

#include "data/Types.hpp"
#include "etl/ETLState.hpp"
#include "etl/SystemState.hpp"
#include "etlng/CacheLoaderInterface.hpp"
#include "etlng/CacheUpdaterInterface.hpp"
#include "etlng/ETLService.hpp"
#include "etlng/ExtractorInterface.hpp"
#include "etlng/InitialLoadObserverInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/MonitorInterface.hpp"
#include "etlng/TaskManagerInterface.hpp"
#include "etlng/TaskManagerProviderInterface.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockAssert.hpp"
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
#include <vector>

using namespace util::config;

namespace {
constinit auto const kSEQ = 100;
constinit auto const kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

struct MockMonitor : public etlng::MonitorInterface {
    MOCK_METHOD(void, notifyLedgerLoaded, (uint32_t), (override));
    MOCK_METHOD(boost::signals2::scoped_connection, subscribe, (SignalType::slot_type const&), (override));
    MOCK_METHOD(void, run, (std::chrono::steady_clock::duration), (override));
    MOCK_METHOD(void, stop, (), (override));
};

struct MockExtractor : etlng::ExtractorInterface {
    MOCK_METHOD(std::optional<etlng::model::LedgerData>, extractLedgerWithDiff, (uint32_t), (override));
    MOCK_METHOD(std::optional<etlng::model::LedgerData>, extractLedgerOnly, (uint32_t), (override));
};

struct MockLoader : etlng::LoaderInterface {
    MOCK_METHOD(void, load, (etlng::model::LedgerData const&), (override));
    MOCK_METHOD(std::optional<ripple::LedgerHeader>, loadInitialLedger, (etlng::model::LedgerData const&), (override));
};

struct MockCacheLoader : etlng::CacheLoaderInterface {
    MOCK_METHOD(void, load, (uint32_t), (override));
    MOCK_METHOD(void, stop, (), (noexcept, override));
    MOCK_METHOD(void, wait, (), (noexcept, override));
};

struct MockCacheUpdater : etlng::CacheUpdaterInterface {
    MOCK_METHOD(void, update, (etlng::model::LedgerData const&), (override));
    MOCK_METHOD(void, update, (uint32_t, std::vector<data::LedgerObject> const&), (override));
    MOCK_METHOD(void, update, (uint32_t, std::vector<etlng::model::Object> const&), (override));
    MOCK_METHOD(void, setFull, (), (override));
};

struct MockInitialLoadObserver : etlng::InitialLoadObserverInterface {
    MOCK_METHOD(
        void,
        onInitialLoadGotMoreObjects,
        (uint32_t, std::vector<etlng::model::Object> const&, std::optional<std::string>),
        (override)
    );
};

struct MockTaskManager : etlng::TaskManagerInterface {
    MOCK_METHOD(void, run, (std::size_t), (override));
    MOCK_METHOD(void, stop, (), (override));
};

struct MockTaskManagerProvider : etlng::TaskManagerProviderInterface {
    MOCK_METHOD(
        std::unique_ptr<etlng::TaskManagerInterface>,
        make,
        (util::async::AnyExecutionContext, std::reference_wrapper<etlng::MonitorInterface>, uint32_t),
        (override)
    );
};

auto
createTestData(uint32_t seq)
{
    auto const header = createLedgerHeader(kLEDGER_HASH, seq);
    return etlng::model::LedgerData{
        .transactions = {},
        .objects = {util::createObject(), util::createObject(), util::createObject()},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = seq
    };
}
}  // namespace

struct ETLServiceTests : util::prometheus::WithPrometheus, MockBackendTest {
    using SameThreadTestContext = util::async::BasicExecutionContext<
        util::async::impl::SameThreadContext,
        util::async::impl::BasicStopSource,
        util::async::impl::SyncDispatchStrategy,
        util::async::impl::SystemContextProvider,
        util::async::impl::NoErrorHandler>;  // This will allow ASSERTs turned exceptions to propagate

protected:
    SameThreadTestContext ctx_;
    util::config::ClioConfigDefinition config_{
        {"extractor_threads", ConfigValue{ConfigType::Integer}.defaultValue(4)},
        {"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2)},
        {"cache.num_diffs", ConfigValue{ConfigType::Integer}.defaultValue(32)},
        {"cache.num_markers", ConfigValue{ConfigType::Integer}.defaultValue(48)},
        {"cache.num_cursors_from_diff", ConfigValue{ConfigType::Integer}.defaultValue(0)},
        {"cache.num_cursors_from_account", ConfigValue{ConfigType::Integer}.defaultValue(0)},
        {"cache.page_fetch_size", ConfigValue{ConfigType::Integer}.defaultValue(512)},
        {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async")}
    };
    StrictMockSubscriptionManagerSharedPtr subscriptions_;
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
    std::shared_ptr<testing::NiceMock<MockExtractor>> extractor_ = std::make_shared<testing::NiceMock<MockExtractor>>();
    std::shared_ptr<testing::NiceMock<MockLoader>> loader_ = std::make_shared<testing::NiceMock<MockLoader>>();
    std::shared_ptr<testing::NiceMock<MockInitialLoadObserver>> initialLoadObserver_ =
        std::make_shared<testing::NiceMock<MockInitialLoadObserver>>();
    std::shared_ptr<testing::NiceMock<MockTaskManagerProvider>> taskManagerProvider_ =
        std::make_shared<testing::NiceMock<MockTaskManagerProvider>>();
    std::shared_ptr<etl::SystemState> systemState_ = std::make_shared<etl::SystemState>();

    etlng::ETLService service_{
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
        systemState_
    };
};

TEST_F(ETLServiceTests, GetInfoWithoutLastPublish)
{
    EXPECT_CALL(*balancer_, toJson()).WillOnce(testing::Return(boost::json::parse(R"JSON([{"test": "value"}])JSON")));
    EXPECT_CALL(*publisher_, getLastPublish()).WillOnce(testing::Return(std::chrono::system_clock::time_point{}));
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
    EXPECT_CALL(*balancer_, toJson()).WillOnce(testing::Return(boost::json::parse(R"JSON([{"test": "value"}])JSON")));
    EXPECT_CALL(*publisher_, getLastPublish()).WillOnce(testing::Return(std::chrono::system_clock::now()));
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
    auto ledgerData = createTestData(kSEQ);

    testing::Sequence const s;
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).InSequence(s).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillRepeatedly(testing::Return(kSEQ));
    EXPECT_CALL(*extractor_, extractLedgerOnly(kSEQ)).WillOnce(testing::Return(ledgerData));
    EXPECT_CALL(*balancer_, loadInitialLedger(kSEQ, testing::_, testing::_))
        .WillOnce(testing::Return(std::vector<std::string>{}));
    EXPECT_CALL(*loader_, loadInitialLedger(testing::_)).WillOnce(testing::Return(ripple::LedgerHeader{}));
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_))
        .InSequence(s)
        .WillOnce(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*mockTaskManager, run(testing::_));
    EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, kSEQ + 1))
        .WillOnce(testing::Return(std::unique_ptr<etlng::TaskManagerInterface>(mockTaskManager.release())));

    service_.run();
}

TEST_F(ETLServiceTests, RunWithPopulatedDatabase)
{
    auto mockTaskManager = std::make_unique<testing::NiceMock<MockTaskManager>>();

    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_))
        .WillRepeatedly(testing::Return(data::LedgerRange{.minSequence = 1, .maxSequence = kSEQ}));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillRepeatedly(testing::Return(kSEQ));
    EXPECT_CALL(*cacheLoader_, load(kSEQ));
    EXPECT_CALL(*mockTaskManager, run(testing::_));
    EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, kSEQ + 1))
        .WillOnce(testing::Return(std::unique_ptr<etlng::TaskManagerInterface>(mockTaskManager.release())));

    service_.run();
}

TEST_F(ETLServiceTests, WaitForValidatedLedgerIsAborted)
{
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).Times(2).WillRepeatedly(testing::Return(std::nullopt));

    // No other calls should happen because we exit early
    EXPECT_CALL(*extractor_, extractLedgerOnly(testing::_)).Times(0);
    EXPECT_CALL(*balancer_, loadInitialLedger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*loader_, loadInitialLedger(testing::_)).Times(0);
    EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, testing::_)).Times(0);

    service_.run();
}

struct ETLServiceAssertTests : common::util::WithMockAssert, ETLServiceTests {};

TEST_F(ETLServiceAssertTests, FailToLoadInitialLedger)
{
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).WillRepeatedly(testing::Return(kSEQ));
    EXPECT_CALL(*extractor_, extractLedgerOnly(kSEQ)).WillOnce(testing::Return(std::nullopt));

    // These calls should not happen because loading the initial ledger fails
    EXPECT_CALL(*balancer_, loadInitialLedger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*loader_, loadInitialLedger(testing::_)).Times(0);
    EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, testing::_)).Times(0);

    EXPECT_CLIO_ASSERT_FAIL({ service_.run(); });
}

TEST_F(ETLServiceAssertTests, WaitForValidatedLedgerIsAbortedLeadToFailToLoadInitialLedger)
{
    testing::Sequence const s;
    EXPECT_CALL(*backend_, hardFetchLedgerRange(testing::_)).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).InSequence(s).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(*ledgers_, getMostRecent()).InSequence(s).WillOnce(testing::Return(kSEQ));

    // No other calls should happen because we exit early
    EXPECT_CALL(*extractor_, extractLedgerOnly(testing::_)).Times(0);
    EXPECT_CALL(*balancer_, loadInitialLedger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*loader_, loadInitialLedger(testing::_)).Times(0);
    EXPECT_CALL(*taskManagerProvider_, make(testing::_, testing::_, testing::_)).Times(0);

    EXPECT_CLIO_ASSERT_FAIL({ service_.run(); });
}
