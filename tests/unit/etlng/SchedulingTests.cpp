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

#include "etlng/Models.hpp"
#include "etlng/SchedulerInterface.hpp"
#include "etlng/impl/Loading.hpp"
#include "etlng/impl/Scheduling.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

using namespace etlng;
using namespace etlng::model;

namespace {
class FakeScheduler : SchedulerInterface {
    std::function<std::optional<Task>()> generator_;

public:
    FakeScheduler(std::function<std::optional<Task>()> generator) : generator_{std::move(generator)}
    {
    }

    [[nodiscard]] std::optional<Task>
    next() override
    {
        return generator_();
    }
};
}  // namespace

struct ForwardSchedulerTests : NoLoggerFixture {
protected:
    MockNetworkValidatedLedgersPtr networkValidatedLedgers_;
};

TEST_F(ForwardSchedulerTests, ExhaustsSchedulerIfMostRecentLedgerIsNewerThanRequestedSequence)
{
    auto scheduler = impl::ForwardScheduler(*networkValidatedLedgers_, 0u, 10u);
    EXPECT_CALL(*networkValidatedLedgers_, getMostRecent()).Times(11).WillRepeatedly(testing::Return(11u));

    for (auto i = 0u; i < 10u; ++i) {
        auto maybeTask = scheduler.next();

        EXPECT_TRUE(maybeTask.has_value());
        EXPECT_EQ(maybeTask->seq, i);
    }

    auto const empty = scheduler.next();
    EXPECT_FALSE(empty.has_value());
}

TEST_F(ForwardSchedulerTests, ReturnsNulloptIfMostRecentLedgerIsOlderThanRequestedSequence)
{
    auto scheduler = impl::ForwardScheduler(*networkValidatedLedgers_, 0u, 10u);
    EXPECT_CALL(*networkValidatedLedgers_, getMostRecent()).Times(10).WillRepeatedly(testing::Return(4u));

    for (auto i = 0u; i < 5u; ++i) {
        auto const maybeTask = scheduler.next();

        EXPECT_TRUE(maybeTask.has_value());
        EXPECT_EQ(maybeTask->seq, i);
    }

    for (auto i = 0u; i < 5u; ++i)
        EXPECT_FALSE(scheduler.next().has_value());
}

TEST(BackfillSchedulerTests, ExhaustsSchedulerUntilMinSeqReached)
{
    auto scheduler = impl::BackfillScheduler(10u, 5u);

    for (auto i = 10u; i > 5u; --i) {
        auto maybeTask = scheduler.next();

        EXPECT_TRUE(maybeTask.has_value());
        EXPECT_EQ(maybeTask->seq, i);
    }

    auto const empty = scheduler.next();
    EXPECT_FALSE(empty.has_value());
}

TEST(BackfillSchedulerTests, ExhaustsSchedulerUntilDefaultMinValueReached)
{
    auto scheduler = impl::BackfillScheduler(10u);

    for (auto i = 10u; i > 0u; --i) {
        auto const maybeTask = scheduler.next();

        EXPECT_TRUE(maybeTask.has_value());
        EXPECT_EQ(maybeTask->seq, i);
    }

    auto const empty = scheduler.next();
    EXPECT_FALSE(empty.has_value());
}

TEST(SchedulerChainTests, ExhaustsOneGenerator)
{
    auto generate = [stop = 10u, seq = 0u]() mutable {
        std::optional<Task> task = std::nullopt;
        if (seq < stop)
            task = Task{.priority = model::Task::Priority::Lower, .seq = seq++};

        return task;
    };
    testing::MockFunction<std::optional<Task>()> upToTenGen;
    EXPECT_CALL(upToTenGen, Call()).Times(11).WillRepeatedly(testing::ByRef(generate));

    auto scheduler = impl::makeScheduler(FakeScheduler(upToTenGen.AsStdFunction()));

    for (auto i = 0u; i < 10u; ++i) {
        auto const maybeTask = scheduler->next();

        EXPECT_TRUE(maybeTask.has_value());
        EXPECT_EQ(maybeTask->seq, i);
    }

    auto const empty = scheduler->next();
    EXPECT_FALSE(empty.has_value());
}

TEST(SchedulerChainTests, ExhaustsFirstSchedulerBeforeUsingSecond)
{
    auto generateFirst = [stop = 10u, seq = 0u]() mutable {
        std::optional<Task> task = std::nullopt;
        if (seq < stop)
            task = Task{.priority = model::Task::Priority::Lower, .seq = seq++};

        return task;
    };
    testing::MockFunction<std::optional<Task>()> upToTenGen;
    EXPECT_CALL(upToTenGen, Call()).Times(21).WillRepeatedly(testing::ByRef(generateFirst));

    auto generateSecond = [seq = 10u]() mutable {
        std::optional<Task> task = std::nullopt;
        if (seq > 0u)
            task = Task{.priority = model::Task::Priority::Lower, .seq = seq--};

        return task;
    };
    testing::MockFunction<std::optional<Task>()> downToZeroGen;
    EXPECT_CALL(downToZeroGen, Call()).Times(11).WillRepeatedly(testing::ByRef(generateSecond));

    auto scheduler =
        impl::makeScheduler(FakeScheduler(upToTenGen.AsStdFunction()), FakeScheduler(downToZeroGen.AsStdFunction()));

    for (auto i = 0u; i < 10u; ++i) {
        auto const maybeTask = scheduler->next();

        EXPECT_TRUE(maybeTask.has_value());
        EXPECT_EQ(maybeTask->seq, i);
    }
    for (auto i = 10u; i > 0u; --i) {
        auto const maybeTask = scheduler->next();

        EXPECT_TRUE(maybeTask.has_value());
        EXPECT_EQ(maybeTask->seq, i);
    }

    auto const empty = scheduler->next();
    EXPECT_FALSE(empty.has_value());
}
