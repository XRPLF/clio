#include "etl/Models.hpp"
#include "etl/SchedulerInterface.hpp"
#include "etl/impl/Loading.hpp"
#include "etl/impl/Scheduling.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

using namespace etl;
using namespace etl::model;

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

struct ForwardSchedulerTests : virtual public ::testing::Test {
protected:
    MockNetworkValidatedLedgersPtr networkValidatedLedgers_;
};

TEST_F(ForwardSchedulerTests, ExhaustsSchedulerIfMostRecentLedgerIsNewerThanRequestedSequence)
{
    auto scheduler = impl::ForwardScheduler(*networkValidatedLedgers_, 0u, 10u);
    EXPECT_CALL(*networkValidatedLedgers_, getMostRecent())
        .Times(11)
        .WillRepeatedly(testing::Return(11u));

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
    EXPECT_CALL(*networkValidatedLedgers_, getMostRecent())
        .Times(10)
        .WillRepeatedly(testing::Return(4u));

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

    auto scheduler = impl::makeScheduler(
        FakeScheduler(upToTenGen.AsStdFunction()), FakeScheduler(downToZeroGen.AsStdFunction())
    );

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
