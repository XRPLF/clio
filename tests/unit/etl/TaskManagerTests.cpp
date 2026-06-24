#include "etl/ExtractorInterface.hpp"
#include "etl/LoaderInterface.hpp"
#include "etl/Models.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/SchedulerInterface.hpp"
#include "etl/impl/Loading.hpp"
#include "etl/impl/TaskManager.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/TestObject.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <boost/signals2/connection.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <semaphore>
#include <vector>

using namespace etl::model;
using namespace etl::impl;

namespace {

constinit auto const kSeq = 30;
constinit auto const kLedgerHash =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

struct MockScheduler : etl::SchedulerInterface {
    MOCK_METHOD(std::optional<Task>, next, (), (override));
};

struct MockExtractor : etl::ExtractorInterface {
    MOCK_METHOD(std::optional<LedgerData>, extractLedgerWithDiff, (uint32_t), (override));
    MOCK_METHOD(std::optional<LedgerData>, extractLedgerOnly, (uint32_t), (override));
};

struct MockLoader : etl::LoaderInterface {
    using ExpectedType = std::expected<void, etl::LoaderError>;
    MOCK_METHOD(ExpectedType, load, (LedgerData const&), (override));
    MOCK_METHOD(
        std::optional<xrpl::LedgerHeader>,
        loadInitialLedger,
        (LedgerData const&),
        (override)
    );
};

struct MockMonitor : etl::MonitorInterface {
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

struct TaskManagerTests : virtual public ::testing::Test {
    using MockSchedulerType = testing::NiceMock<MockScheduler>;
    using MockExtractorType = testing::NiceMock<MockExtractor>;
    using MockLoaderType = testing::NiceMock<MockLoader>;
    using MockMonitorType = testing::NiceMock<MockMonitor>;

protected:
    util::async::CoroExecutionContext ctx_{2};
    std::shared_ptr<MockSchedulerType> mockSchedulerPtr_ = std::make_shared<MockSchedulerType>();
    std::shared_ptr<MockExtractorType> mockExtractorPtr_ = std::make_shared<MockExtractorType>();
    std::shared_ptr<MockLoaderType> mockLoaderPtr_ = std::make_shared<MockLoaderType>();
    std::shared_ptr<MockMonitorType> mockMonitorPtr_ = std::make_shared<MockMonitorType>();

    TaskManager taskManager_{
        ctx_,
        mockSchedulerPtr_,
        *mockExtractorPtr_,
        *mockLoaderPtr_,
        *mockMonitorPtr_,
        kSeq
    };
};

auto
createTestData(uint32_t seq)
{
    auto const header = createLedgerHeader(kLedgerHash, seq);
    return LedgerData{
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

TEST_F(TaskManagerTests, LoaderGetsDataIfNextSequenceIsExtracted)
{
    static constexpr auto kTotal = 64uz;
    static constexpr auto kExtractors = 4uz;

    std::atomic_uint32_t seq = kSeq;
    std::vector<uint32_t> loaded;
    std::binary_semaphore done{0};

    EXPECT_CALL(*mockSchedulerPtr_, next()).WillRepeatedly([&]() {
        return Task{.priority = Task::Priority::Higher, .seq = seq++};
    });

    EXPECT_CALL(*mockExtractorPtr_, extractLedgerWithDiff(testing::_))
        .WillRepeatedly([](uint32_t seq) -> std::optional<LedgerData> {
            if (seq > kSeq + kTotal - 1)
                return std::nullopt;

            return createTestData(seq);
        });

    EXPECT_CALL(*mockLoaderPtr_, load(testing::_))
        .Times(kTotal)
        .WillRepeatedly([&](LedgerData data) -> std::expected<void, etl::LoaderError> {
            loaded.push_back(data.seq);
            if (loaded.size() == kTotal)
                done.release();

            return {};
        });

    EXPECT_CALL(*mockMonitorPtr_, notifySequenceLoaded(testing::_)).Times(kTotal);

    taskManager_.run(kExtractors);
    done.acquire();
    taskManager_.stop();

    EXPECT_EQ(loaded.size(), kTotal);
    for (std::size_t i = 0; i < loaded.size(); ++i)
        EXPECT_EQ(loaded[i], kSeq + i);
}

TEST_F(TaskManagerTests, WriteConflictHandling)
{
    static constexpr auto kTotal = 64uz;
    static constexpr auto kConflictAfter = 32uz;  // Conflict after 32 ledgers
    static constexpr auto kExtractors = 4uz;

    std::atomic_uint32_t seq = kSeq;
    std::vector<uint32_t> loaded;
    std::binary_semaphore done{0};
    bool conflictOccurred = false;

    EXPECT_CALL(*mockSchedulerPtr_, next()).WillRepeatedly([&]() {
        return Task{.priority = Task::Priority::Higher, .seq = seq++};
    });

    EXPECT_CALL(*mockExtractorPtr_, extractLedgerWithDiff(testing::_))
        .WillRepeatedly([](uint32_t seq) -> std::optional<LedgerData> {
            if (seq > kSeq + kTotal - 1)
                return std::nullopt;

            return createTestData(seq);
        });

    // First kConflictAfter calls succeed, then we get a write conflict
    EXPECT_CALL(*mockLoaderPtr_, load(testing::_))
        .WillRepeatedly([&](LedgerData data) -> std::expected<void, etl::LoaderError> {
            loaded.push_back(data.seq);

            if (loaded.size() == kConflictAfter) {
                conflictOccurred = true;
                done.release();
                return std::unexpected(etl::LoaderError::WriteConflict);
            }

            if (loaded.size() == kTotal)
                done.release();

            return {};
        });

    EXPECT_CALL(*mockMonitorPtr_, notifySequenceLoaded(testing::_)).Times(kConflictAfter - 1);
    EXPECT_CALL(*mockMonitorPtr_, notifyWriteConflict(kSeq + kConflictAfter - 1));

    taskManager_.run(kExtractors);
    done.acquire();
    taskManager_.stop();

    EXPECT_EQ(loaded.size(), kConflictAfter);
    EXPECT_TRUE(conflictOccurred);

    for (std::size_t i = 0; i < loaded.size(); ++i)
        EXPECT_EQ(loaded[i], kSeq + i);
}

TEST_F(TaskManagerTests, AmendmentBlockedHandling)
{
    static constexpr auto kTotal = 64uz;
    static constexpr auto kAmendmentBlockedAfter = 20uz;  // Amendment block after 20 ledgers
    static constexpr auto kExtractors = 2uz;

    std::atomic_uint32_t seq = kSeq;
    std::vector<uint32_t> loaded;
    std::binary_semaphore done{0};
    bool amendmentBlockedOccurred = false;

    EXPECT_CALL(*mockSchedulerPtr_, next()).WillRepeatedly([&]() {
        return Task{.priority = Task::Priority::Higher, .seq = seq++};
    });

    EXPECT_CALL(*mockExtractorPtr_, extractLedgerWithDiff(testing::_))
        .WillRepeatedly([](uint32_t seq) -> std::optional<LedgerData> {
            if (seq > kSeq + kTotal - 1)
                return std::nullopt;

            return createTestData(seq);
        });

    EXPECT_CALL(*mockLoaderPtr_, load(testing::_))
        .WillRepeatedly([&](LedgerData data) -> std::expected<void, etl::LoaderError> {
            loaded.push_back(data.seq);

            if (loaded.size() == kAmendmentBlockedAfter) {
                amendmentBlockedOccurred = true;
                done.release();
                return std::unexpected(etl::LoaderError::AmendmentBlocked);
            }

            if (loaded.size() == kTotal)
                done.release();

            return {};
        });

    EXPECT_CALL(*mockMonitorPtr_, notifySequenceLoaded(testing::_))
        .Times(kAmendmentBlockedAfter - 1);
    EXPECT_CALL(*mockMonitorPtr_, notifyWriteConflict(testing::_)).Times(0);

    taskManager_.run(kExtractors);
    done.acquire();
    taskManager_.stop();

    EXPECT_EQ(loaded.size(), kAmendmentBlockedAfter);
    EXPECT_TRUE(amendmentBlockedOccurred);

    for (std::size_t i = 0; i < loaded.size(); ++i)
        EXPECT_EQ(loaded[i], kSeq + i);
}
