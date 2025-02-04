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

#include "etlng/ExtractorInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/SchedulerInterface.hpp"
#include "etlng/impl/Loading.hpp"
#include "etlng/impl/TaskManager.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/TestObject.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <semaphore>
#include <vector>

using namespace etlng::model;
using namespace etlng::impl;

namespace {

constinit auto const kSEQ = 30;
constinit auto const kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

struct MockScheduler : etlng::SchedulerInterface {
    MOCK_METHOD(std::optional<Task>, next, (), (override));
};

struct MockExtractor : etlng::ExtractorInterface {
    MOCK_METHOD(std::optional<LedgerData>, extractLedgerWithDiff, (uint32_t), (override));
    MOCK_METHOD(std::optional<LedgerData>, extractLedgerOnly, (uint32_t), (override));
};

struct MockLoader : etlng::LoaderInterface {
    MOCK_METHOD(void, load, (LedgerData const&), (override));
    MOCK_METHOD(std::optional<ripple::LedgerHeader>, loadInitialLedger, (LedgerData const&), (override));
};

struct TaskManagerTests : NoLoggerFixture {
    using MockSchedulerType = testing::NiceMock<MockScheduler>;
    using MockExtractorType = testing::NiceMock<MockExtractor>;
    using MockLoaderType = testing::NiceMock<MockLoader>;

protected:
    util::async::CoroExecutionContext ctx_{2};
    std::shared_ptr<MockSchedulerType> mockSchedulerPtr_ = std::make_shared<MockSchedulerType>();
    std::shared_ptr<MockExtractorType> mockExtractorPtr_ = std::make_shared<MockExtractorType>();
    std::shared_ptr<MockLoaderType> mockLoaderPtr_ = std::make_shared<MockLoaderType>();

    TaskManager taskManager_{ctx_, *mockSchedulerPtr_, *mockExtractorPtr_, *mockLoaderPtr_};
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
    static constexpr auto kEXTRACTORS = 5uz;
    static constexpr auto kLOADERS = 1uz;

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

    EXPECT_CALL(*mockLoaderPtr_, load(testing::_)).Times(kTOTAL).WillRepeatedly([&](LedgerData data) {
        loaded.push_back(data.seq);

        if (loaded.size() == kTOTAL) {
            done.release();
        }
    });

    auto loop = ctx_.execute([&] { taskManager_.run({.numExtractors = kEXTRACTORS, .numLoaders = kLOADERS}); });
    done.acquire();

    taskManager_.stop();
    loop.wait();

    EXPECT_EQ(loaded.size(), kTOTAL);
    for (std::size_t i = 0; i < loaded.size(); ++i) {
        EXPECT_EQ(loaded[i], kSEQ + i);
    }
}
