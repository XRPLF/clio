//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

constinit auto const kSEQ = 30;
constinit auto const kLEDGER_HASH =
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
        std::optional<ripple::LedgerHeader>,
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
        kSEQ
    };
};

auto
createTestData(uint32_t seq)
{
    auto const header = createLedgerHeader(kLEDGER_HASH, seq);
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
    static constexpr auto kTOTAL = 64uz;
    static constexpr auto kEXTRACTORS = 4uz;

    std::atomic_uint32_t seq = kSEQ;
    std::vector<uint32_t> loaded;
    std::binary_semaphore done{0};

    EXPECT_CALL(*mockSchedulerPtr_, next()).WillRepeatedly([&]() {
        return Task{.priority = Task::Priority::Higher, .seq = seq++};
    });

    EXPECT_CALL(*mockExtractorPtr_, extractLedgerWithDiff(testing::_))
        .WillRepeatedly([](uint32_t seq) -> std::optional<LedgerData> {
            if (seq > kSEQ + kTOTAL - 1)
                return std::nullopt;

            return createTestData(seq);
        });

    EXPECT_CALL(*mockLoaderPtr_, load(testing::_))
        .Times(kTOTAL)
        .WillRepeatedly([&](LedgerData data) -> std::expected<void, etl::LoaderError> {
            loaded.push_back(data.seq);
            if (loaded.size() == kTOTAL)
                done.release();

            return {};
        });

    EXPECT_CALL(*mockMonitorPtr_, notifySequenceLoaded(testing::_)).Times(kTOTAL);

    taskManager_.run(kEXTRACTORS);
    done.acquire();
    taskManager_.stop();

    EXPECT_EQ(loaded.size(), kTOTAL);
    for (std::size_t i = 0; i < loaded.size(); ++i)
        EXPECT_EQ(loaded[i], kSEQ + i);
}

TEST_F(TaskManagerTests, WriteConflictHandling)
{
    static constexpr auto kTOTAL = 64uz;
    static constexpr auto kCONFLICT_AFTER = 32uz;  // Conflict after 32 ledgers
    static constexpr auto kEXTRACTORS = 4uz;

    std::atomic_uint32_t seq = kSEQ;
    std::vector<uint32_t> loaded;
    std::binary_semaphore done{0};
    bool conflictOccurred = false;

    EXPECT_CALL(*mockSchedulerPtr_, next()).WillRepeatedly([&]() {
        return Task{.priority = Task::Priority::Higher, .seq = seq++};
    });

    EXPECT_CALL(*mockExtractorPtr_, extractLedgerWithDiff(testing::_))
        .WillRepeatedly([](uint32_t seq) -> std::optional<LedgerData> {
            if (seq > kSEQ + kTOTAL - 1)
                return std::nullopt;

            return createTestData(seq);
        });

    // First kCONFLICT_AFTER calls succeed, then we get a write conflict
    EXPECT_CALL(*mockLoaderPtr_, load(testing::_))
        .WillRepeatedly([&](LedgerData data) -> std::expected<void, etl::LoaderError> {
            loaded.push_back(data.seq);

            if (loaded.size() == kCONFLICT_AFTER) {
                conflictOccurred = true;
                done.release();
                return std::unexpected(etl::LoaderError::WriteConflict);
            }

            if (loaded.size() == kTOTAL)
                done.release();

            return {};
        });

    EXPECT_CALL(*mockMonitorPtr_, notifySequenceLoaded(testing::_)).Times(kCONFLICT_AFTER - 1);
    EXPECT_CALL(*mockMonitorPtr_, notifyWriteConflict(kSEQ + kCONFLICT_AFTER - 1));

    taskManager_.run(kEXTRACTORS);
    done.acquire();
    taskManager_.stop();

    EXPECT_EQ(loaded.size(), kCONFLICT_AFTER);
    EXPECT_TRUE(conflictOccurred);

    for (std::size_t i = 0; i < loaded.size(); ++i)
        EXPECT_EQ(loaded[i], kSEQ + i);
}

TEST_F(TaskManagerTests, AmendmentBlockedHandling)
{
    static constexpr auto kTOTAL = 64uz;
    static constexpr auto kAMENDMENT_BLOCKED_AFTER = 20uz;  // Amendment block after 20 ledgers
    static constexpr auto kEXTRACTORS = 2uz;

    std::atomic_uint32_t seq = kSEQ;
    std::vector<uint32_t> loaded;
    std::binary_semaphore done{0};
    bool amendmentBlockedOccurred = false;

    EXPECT_CALL(*mockSchedulerPtr_, next()).WillRepeatedly([&]() {
        return Task{.priority = Task::Priority::Higher, .seq = seq++};
    });

    EXPECT_CALL(*mockExtractorPtr_, extractLedgerWithDiff(testing::_))
        .WillRepeatedly([](uint32_t seq) -> std::optional<LedgerData> {
            if (seq > kSEQ + kTOTAL - 1)
                return std::nullopt;

            return createTestData(seq);
        });

    EXPECT_CALL(*mockLoaderPtr_, load(testing::_))
        .WillRepeatedly([&](LedgerData data) -> std::expected<void, etl::LoaderError> {
            loaded.push_back(data.seq);

            if (loaded.size() == kAMENDMENT_BLOCKED_AFTER) {
                amendmentBlockedOccurred = true;
                done.release();
                return std::unexpected(etl::LoaderError::AmendmentBlocked);
            }

            if (loaded.size() == kTOTAL)
                done.release();

            return {};
        });

    EXPECT_CALL(*mockMonitorPtr_, notifySequenceLoaded(testing::_))
        .Times(kAMENDMENT_BLOCKED_AFTER - 1);
    EXPECT_CALL(*mockMonitorPtr_, notifyWriteConflict(testing::_)).Times(0);

    taskManager_.run(kEXTRACTORS);
    done.acquire();
    taskManager_.stop();

    EXPECT_EQ(loaded.size(), kAMENDMENT_BLOCKED_AFTER);
    EXPECT_TRUE(amendmentBlockedOccurred);

    for (std::size_t i = 0; i < loaded.size(); ++i)
        EXPECT_EQ(loaded[i], kSEQ + i);
}
