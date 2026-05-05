#include "util/Assert.hpp"
#include "util/Channel.hpp"
#include "util/Mutex.hpp"
#include "util/OverloadSet.hpp"
#include "util/Spawn.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system/detail/error_code.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <semaphore>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace testing;

namespace {

constexpr auto kDEFAULT_THREAD_POOL_SIZE = 4;
constexpr auto kTEST_TIMEOUT = std::chrono::seconds{10};

constexpr auto kNUM_SENDERS = 3uz;
constexpr auto kNUM_RECEIVERS = 3uz;
constexpr auto kVALUES_PER_SENDER = 500uz;
constexpr auto kTOTAL_EXPECTED = kNUM_SENDERS * kVALUES_PER_SENDER;

enum class ContextType { IOContext, ThreadPool };

constexpr int
generateValue(std::size_t senderId, std::size_t i)
{
    return static_cast<int>((senderId * 100) + i);
}

std::vector<int>
generateExpectedValues()
{
    std::vector<int> expectedValues;
    expectedValues.reserve(kTOTAL_EXPECTED);
    for (auto senderId = 0uz; senderId < kNUM_SENDERS; ++senderId) {
        for (auto i = 0uz; i < kVALUES_PER_SENDER; ++i) {
            expectedValues.push_back(generateValue(senderId, i));
        }
    }
    std::ranges::sort(expectedValues);
    return expectedValues;
}

std::vector<int> const kEXPECTED_VALUES = generateExpectedValues();

std::string
contextTypeToString(ContextType type)
{
    return type == ContextType::IOContext ? "IOContext" : "ThreadPool";
}

class ContextWrapper {
public:
    using ContextVariant = std::variant<boost::asio::io_context, boost::asio::thread_pool>;

    explicit ContextWrapper(ContextType type)
        : context_([type] {
            if (type == ContextType::IOContext)
                return ContextVariant(std::in_place_type_t<boost::asio::io_context>());

            if (type == ContextType::ThreadPool) {
                return ContextVariant(
                    std::in_place_type_t<boost::asio::thread_pool>(), kDEFAULT_THREAD_POOL_SIZE
                );
            }

            ASSERT(false, "Unknown new type of context");
            std::unreachable();
        }())
    {
    }

    template <typename Fn>
    void
    withExecutor(Fn&& fn)
    {
        std::visit(std::forward<Fn>(fn), context_);
    }

    void
    run()
    {
        std::visit(
            util::OverloadSet{
                [](boost::asio::io_context& context) { context.run_for(kTEST_TIMEOUT); },
                [](boost::asio::thread_pool& context) { context.join(); },
            },
            context_
        );
    }

private:
    ContextVariant context_;
};

}  // namespace

class ChannelSpawnTest : public TestWithParam<ContextType> {
protected:
    ChannelSpawnTest() : context_(GetParam())
    {
    }

    ContextWrapper context_;
};

class ChannelCallbackTest : public TestWithParam<ContextType> {
protected:
    ChannelCallbackTest() : context_(GetParam())
    {
    }

    ContextWrapper context_;
};

TEST_P(ChannelSpawnTest, MultipleSendersOneReceiver)
{
    context_.withExecutor([this](auto& executor) {
        auto [sender, receiver] = util::Channel<int>::create(executor, 10);
        util::Mutex<std::vector<int>> receivedValues;

        util::spawn(
            executor, [&receiver, &receivedValues](boost::asio::yield_context yield) mutable {
                while (true) {
                    auto value = receiver.asyncReceive(yield);
                    if (not value.has_value())
                        break;
                    receivedValues.lock()->push_back(*value);
                }
            }
        );

        {
            auto localSender = std::move(sender);
            for (auto senderId = 0uz; senderId < kNUM_SENDERS; ++senderId) {
                util::spawn(
                    executor,
                    [senderCopy = localSender, senderId](boost::asio::yield_context yield) mutable {
                        for (auto i = 0uz; i < kVALUES_PER_SENDER; ++i) {
                            if (not senderCopy.asyncSend(generateValue(senderId, i), yield))
                                break;
                        }
                    }
                );
            }
        }

        context_.run();

        EXPECT_EQ(receivedValues.lock()->size(), kTOTAL_EXPECTED);
        std::ranges::sort(receivedValues.lock().get());

        EXPECT_EQ(receivedValues.lock().get(), kEXPECTED_VALUES);
    });
}

TEST_P(ChannelSpawnTest, MultipleSendersMultipleReceivers)
{
    context_.withExecutor([this](auto& executor) {
        auto [sender, receiver] = util::Channel<int>::create(executor, 10);
        util::Mutex<std::vector<int>> receivedValues;
        std::vector receivers(kNUM_RECEIVERS, receiver);

        for (auto receiverId = 0uz; receiverId < kNUM_RECEIVERS; ++receiverId) {
            util::spawn(
                executor,
                [&receiverRef = receivers[receiverId],
                 &receivedValues](boost::asio::yield_context yield) mutable {
                    while (true) {
                        auto value = receiverRef.asyncReceive(yield);
                        if (not value.has_value())
                            break;
                        receivedValues.lock()->push_back(*value);
                    }
                }
            );
        }

        {
            auto localSender = std::move(sender);
            for (auto senderId = 0uz; senderId < kNUM_SENDERS; ++senderId) {
                util::spawn(
                    executor,
                    [senderCopy = localSender, senderId](boost::asio::yield_context yield) mutable {
                        for (auto i = 0uz; i < kVALUES_PER_SENDER; ++i) {
                            auto const value = generateValue(senderId, i);
                            if (not senderCopy.asyncSend(value, yield))
                                break;
                        }
                    }
                );
            }
        }

        context_.run();

        EXPECT_EQ(receivedValues.lock()->size(), kTOTAL_EXPECTED);
        std::ranges::sort(receivedValues.lock().get());

        EXPECT_EQ(receivedValues.lock().get(), kEXPECTED_VALUES);
    });
}

TEST_P(ChannelSpawnTest, ChannelClosureScenarios)
{
    context_.withExecutor([this](auto& executor) {
        std::atomic_bool testCompleted{false};

        util::spawn(
            executor, [&executor, &testCompleted](boost::asio::yield_context yield) mutable {
                auto [sender, receiver] = util::Channel<int>::create(executor, 5);

                EXPECT_FALSE(receiver.isClosed());

                bool const success = sender.asyncSend(42, yield);
                EXPECT_TRUE(success);

                auto value = receiver.asyncReceive(yield);
                EXPECT_TRUE(value.has_value());
                EXPECT_EQ(*value, 42);

                {
                    [[maybe_unused]] auto tempSender = std::move(sender);
                }

                EXPECT_TRUE(receiver.isClosed());

                auto closedValue = receiver.asyncReceive(yield);
                EXPECT_FALSE(closedValue.has_value());

                testCompleted = true;
            }
        );

        context_.run();
        EXPECT_TRUE(testCompleted);
    });
}

TEST_P(ChannelSpawnTest, TrySendTryReceiveMethods)
{
    context_.withExecutor([this](auto& executor) {
        std::atomic_bool testCompleted{false};

        util::spawn(executor, [&executor, &testCompleted](boost::asio::yield_context) mutable {
            auto [sender, receiver] = util::Channel<int>::create(executor, 3);

            EXPECT_FALSE(receiver.tryReceive().has_value());

            EXPECT_TRUE(sender.trySend(42));
            EXPECT_TRUE(sender.trySend(43));
            EXPECT_TRUE(sender.trySend(44));
            EXPECT_FALSE(sender.trySend(45));  // channel full

            auto value1 = receiver.tryReceive();
            EXPECT_TRUE(value1.has_value());
            EXPECT_EQ(*value1, 42);

            auto value2 = receiver.tryReceive();
            EXPECT_TRUE(value2.has_value());
            EXPECT_EQ(*value2, 43);

            EXPECT_TRUE(sender.trySend(46));

            auto value3 = receiver.tryReceive();
            EXPECT_TRUE(value3.has_value());
            EXPECT_EQ(*value3, 44);

            auto value4 = receiver.tryReceive();
            EXPECT_TRUE(value4.has_value());
            EXPECT_EQ(*value4, 46);

            EXPECT_FALSE(receiver.tryReceive().has_value());

            testCompleted = true;
        });

        context_.run();
        EXPECT_TRUE(testCompleted);
    });
}

TEST_P(ChannelSpawnTest, TryMethodsWithClosedChannel)
{
    context_.withExecutor([this](auto& executor) {
        std::atomic_bool testCompleted{false};

        util::spawn(executor, [&executor, &testCompleted](boost::asio::yield_context) mutable {
            auto [sender, receiver] = util::Channel<int>::create(executor, 3);

            EXPECT_TRUE(sender.trySend(42));
            EXPECT_TRUE(sender.trySend(43));

            {
                [[maybe_unused]] auto tempSender = std::move(sender);
            }

            EXPECT_TRUE(receiver.isClosed());

            auto value1 = receiver.tryReceive();
            EXPECT_TRUE(value1.has_value());
            EXPECT_EQ(*value1, 42);

            auto value2 = receiver.tryReceive();
            EXPECT_TRUE(value2.has_value());
            EXPECT_EQ(*value2, 43);

            EXPECT_FALSE(receiver.tryReceive().has_value());

            testCompleted = true;
        });

        context_.run();
        EXPECT_TRUE(testCompleted);
    });
}

INSTANTIATE_TEST_SUITE_P(
    SpawnTests,
    ChannelSpawnTest,
    Values(ContextType::IOContext, ContextType::ThreadPool),
    [](TestParamInfo<ContextType> const& info) { return contextTypeToString(info.param); }
);

TEST_P(ChannelCallbackTest, MultipleSendersOneReceiver)
{
    context_.withExecutor([this](auto& executor) {
        auto [sender, receiver] = util::Channel<int>::create(executor, 10);
        util::Mutex<std::vector<int>> receivedValues;

        auto receiveNext = [&receiver, &receivedValues](this auto&& self) -> void {
            if (receivedValues.lock()->size() >= kTOTAL_EXPECTED)
                return;

            receiver.asyncReceive([&receivedValues,
                                   self = std::forward<decltype(self)>(self)](auto value) {
                if (value.has_value()) {
                    receivedValues.lock()->push_back(*value);
                    self();
                }
            });
        };

        boost::asio::post(executor, receiveNext);

        {
            auto localSender = std::move(sender);
            for (auto senderId = 0uz; senderId < kNUM_SENDERS; ++senderId) {
                auto senderCopy = localSender;
                boost::asio::post(
                    executor, [senderCopy = std::move(senderCopy), senderId, &executor]() mutable {
                        auto sendNext = [senderCopy = std::move(senderCopy),
                                         senderId,
                                         &executor](this auto&& self, std::size_t i) -> void {
                            if (i >= kVALUES_PER_SENDER)
                                return;

                            senderCopy.asyncSend(
                                generateValue(senderId, i),
                                [self = std::forward<decltype(self)>(self),
                                 &executor,
                                 i](bool success) mutable {
                                    if (success) {
                                        boost::asio::post(
                                            executor,
                                            [self = std::move(self), i]() mutable { self(i + 1); }
                                        );
                                    }
                                }
                            );
                        };
                        sendNext(0);
                    }
                );
            }
        }

        context_.run();

        EXPECT_EQ(receivedValues.lock()->size(), kTOTAL_EXPECTED);
        std::ranges::sort(receivedValues.lock().get());

        EXPECT_EQ(receivedValues.lock().get(), kEXPECTED_VALUES);
    });
}

TEST_P(ChannelCallbackTest, MultipleSendersMultipleReceivers)
{
    context_.withExecutor([this](auto& executor) {
        auto [sender, receiver] = util::Channel<int>::create(executor, 10);
        util::Mutex<std::vector<int>> receivedValues;
        std::vector receivers(kNUM_RECEIVERS, receiver);

        for (auto receiverId = 0uz; receiverId < kNUM_RECEIVERS; ++receiverId) {
            auto& receiverRef = receivers[receiverId];
            auto receiveNext = [&receiverRef, &receivedValues](this auto&& self) -> void {
                receiverRef.asyncReceive([&receivedValues,
                                          self = std::forward<decltype(self)>(self)](auto value) {
                    if (value.has_value()) {
                        receivedValues.lock()->push_back(*value);
                        self();
                    }
                });
            };
            boost::asio::post(executor, receiveNext);
        }

        {
            auto localSender = std::move(sender);
            for (auto senderId = 0uz; senderId < kNUM_SENDERS; ++senderId) {
                auto senderCopy = localSender;
                boost::asio::post(
                    executor, [senderCopy = std::move(senderCopy), senderId, &executor]() mutable {
                        auto sendNext = [senderCopy = std::move(senderCopy),
                                         senderId,
                                         &executor](this auto&& self, std::size_t i) -> void {
                            if (i >= kVALUES_PER_SENDER)
                                return;

                            senderCopy.asyncSend(
                                generateValue(senderId, i),
                                [self = std::forward<decltype(self)>(self),
                                 &executor,
                                 i](bool success) mutable {
                                    if (success) {
                                        boost::asio::post(
                                            executor,
                                            [self = std::move(self), i]() mutable { self(i + 1); }
                                        );
                                    }
                                }
                            );
                        };
                        sendNext(0);
                    }
                );
            }
        }

        context_.run();

        EXPECT_EQ(receivedValues.lock()->size(), kTOTAL_EXPECTED);
        std::ranges::sort(receivedValues.lock().get());

        EXPECT_EQ(receivedValues.lock().get(), kEXPECTED_VALUES);
    });
}

TEST_P(ChannelCallbackTest, ChannelClosureScenarios)
{
    context_.withExecutor([this](auto& executor) {
        std::atomic_bool testCompleted{false};
        auto [sender, receiver] = util::Channel<int>::create(executor, 5);
        auto receiverPtr = std::make_shared<decltype(receiver)>(std::move(receiver));
        auto senderPtr = std::make_shared<std::optional<decltype(sender)>>(std::move(sender));

        EXPECT_FALSE(receiverPtr->isClosed());

        senderPtr->value().asyncSend(
            42, [&executor, receiverPtr, senderPtr, &testCompleted](bool success) {
                EXPECT_TRUE(success);

                receiverPtr->asyncReceive(
                    [&executor, receiverPtr, senderPtr, &testCompleted](auto value) {
                        EXPECT_TRUE(value.has_value());
                        EXPECT_EQ(*value, 42);

                        boost::asio::post(
                            executor, [&executor, receiverPtr, senderPtr, &testCompleted]() {
                                (*senderPtr).reset();
                                EXPECT_TRUE(receiverPtr->isClosed());

                                boost::asio::post(executor, [receiverPtr, &testCompleted]() {
                                    receiverPtr->asyncReceive([&testCompleted](auto closedValue) {
                                        EXPECT_FALSE(closedValue.has_value());
                                        testCompleted = true;
                                    });
                                });
                            }
                        );
                    }
                );
            }
        );

        context_.run();
        EXPECT_TRUE(testCompleted);
    });
}

TEST_P(ChannelCallbackTest, TrySendTryReceiveMethods)
{
    context_.withExecutor([this](auto& executor) {
        std::atomic_bool testCompleted{false};
        auto [sender, receiver] = util::Channel<int>::create(executor, 2);
        auto receiverPtr = std::make_shared<decltype(receiver)>(std::move(receiver));
        auto senderPtr = std::make_shared<decltype(sender)>(std::move(sender));

        boost::asio::post(executor, [receiverPtr, senderPtr, &testCompleted]() {
            EXPECT_FALSE(receiverPtr->tryReceive().has_value());

            EXPECT_TRUE(senderPtr->trySend(100));
            EXPECT_TRUE(senderPtr->trySend(101));
            EXPECT_FALSE(senderPtr->trySend(102));  // channel full

            auto value1 = receiverPtr->tryReceive();
            EXPECT_TRUE(value1.has_value());
            EXPECT_EQ(*value1, 100);

            EXPECT_TRUE(senderPtr->trySend(103));

            auto value2 = receiverPtr->tryReceive();
            EXPECT_TRUE(value2.has_value());
            EXPECT_EQ(*value2, 101);

            auto value3 = receiverPtr->tryReceive();
            EXPECT_TRUE(value3.has_value());
            EXPECT_EQ(*value3, 103);

            testCompleted = true;
        });

        context_.run();
        EXPECT_TRUE(testCompleted);
    });
}

TEST_P(ChannelCallbackTest, TryMethodsWithClosedChannel)
{
    context_.withExecutor([this](auto& executor) {
        std::atomic_bool testCompleted{false};
        auto [sender, receiver] = util::Channel<int>::create(executor, 3);
        auto receiverPtr = std::make_shared<util::Channel<int>::Receiver>(std::move(receiver));
        auto senderPtr =
            std::make_shared<std::optional<util::Channel<int>::Sender>>(std::move(sender));

        boost::asio::post(executor, [receiverPtr, senderPtr, &testCompleted]() {
            EXPECT_TRUE(senderPtr->value().trySend(100));
            EXPECT_TRUE(senderPtr->value().trySend(101));

            (*senderPtr).reset();

            EXPECT_TRUE(receiverPtr->isClosed());

            auto value1 = receiverPtr->tryReceive();
            EXPECT_TRUE(value1.has_value());
            EXPECT_EQ(*value1, 100);

            auto value2 = receiverPtr->tryReceive();
            EXPECT_TRUE(value2.has_value());
            EXPECT_EQ(*value2, 101);

            EXPECT_FALSE(receiverPtr->tryReceive().has_value());

            testCompleted = true;
        });

        context_.run();
        EXPECT_TRUE(testCompleted);
    });
}

INSTANTIATE_TEST_SUITE_P(
    CallbackTests,
    ChannelCallbackTest,
    Values(ContextType::IOContext, ContextType::ThreadPool),
    [](TestParamInfo<ContextType> const& info) { return contextTypeToString(info.param); }
);

TEST(ChannelTest, MultipleSenderCopiesErrorHandling)
{
    boost::asio::io_context executor;
    bool testCompleted = false;

    util::spawn(executor, [&executor, &testCompleted](boost::asio::yield_context yield) mutable {
        auto [sender, receiver] = util::Channel<int>::create(executor, 5);

        bool const success = sender.asyncSend(42, yield);
        EXPECT_TRUE(success);

        auto value = receiver.asyncReceive(yield);
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(*value, 42);

        auto senderCopy = sender;
        {
            [[maybe_unused]] auto tempSender = std::move(sender);
            // tempSender destroyed here, but senderCopy still exists
        }

        EXPECT_FALSE(receiver.isClosed());

        {
            [[maybe_unused]] auto tempSender = std::move(senderCopy);
            // now all senders are destroyed, channel should close
        }

        EXPECT_TRUE(receiver.isClosed());

        auto closedValue = receiver.asyncReceive(yield);
        EXPECT_FALSE(closedValue.has_value());

        testCompleted = true;
    });

    executor.run_for(kTEST_TIMEOUT);
    EXPECT_TRUE(testCompleted);
}

TEST(ChannelTest, ChannelClosesWhenAllSendersDestroyed)
{
    boost::asio::io_context executor;
    auto [sender, receiver] = util::Channel<int>::create(executor, 5);

    EXPECT_FALSE(receiver.isClosed());

    auto senderCopy = sender;
    {
        [[maybe_unused]] auto temp = std::move(sender);
    }
    EXPECT_FALSE(receiver.isClosed());  // one sender still exists

    {
        [[maybe_unused]] auto temp = std::move(senderCopy);
    }
    EXPECT_TRUE(receiver.isClosed());  // all senders destroyed
}

TEST(ChannelTest, ChannelClosesWhenAllReceiversDestroyed)
{
    boost::asio::io_context executor;
    auto [sender, receiver] = util::Channel<int>::create(executor, 5);

    EXPECT_TRUE(sender.trySend(42));

    auto receiverCopy = receiver;
    {
        [[maybe_unused]] auto temp = std::move(receiver);
    }
    EXPECT_TRUE(sender.trySend(43));  // one receiver still exists, can send

    {
        [[maybe_unused]] auto temp = std::move(receiverCopy);
    }
    EXPECT_FALSE(sender.trySend(44));  // all receivers destroyed, channel closed
}

TEST(ChannelTest, ChannelPreservesOrderFIFO)
{
    boost::asio::io_context executor;
    bool testCompleted = false;
    std::vector<int> const valuesToSend = {42, 7, 99, 13, 5, 88, 21, 3, 67, 54};

    util::spawn(
        executor,
        [&executor, &testCompleted, &valuesToSend](boost::asio::yield_context yield) mutable {
            auto [sender, receiver] = util::Channel<int>::create(executor, 5);
            std::vector<int> receivedValues;

            // Spawn a receiver coroutine that collects all values
            util::spawn(
                executor, [&receiver, &receivedValues](boost::asio::yield_context yield) mutable {
                    auto value = receiver.asyncReceive(yield);
                    while (value.has_value()) {
                        receivedValues.push_back(*value);
                        value = receiver.asyncReceive(yield);
                    }
                }
            );

            // Send all values
            for (int const value : valuesToSend) {
                EXPECT_TRUE(sender.asyncSend(value, yield));
            }

            // Close sender to signal end of data
            {
                [[maybe_unused]] auto temp = std::move(sender);
            }

            // Give receiver time to process all values
            boost::asio::steady_timer timer(executor, std::chrono::milliseconds{50});
            timer.async_wait(yield);

            // Verify received values match sent values in the same order
            EXPECT_EQ(receivedValues, valuesToSend);

            testCompleted = true;
        }
    );

    executor.run_for(kTEST_TIMEOUT);
    EXPECT_TRUE(testCompleted);
}

TEST(ChannelTest, AsyncReceiveWakesUpWhenSenderDestroyed)
{
    boost::asio::io_context executor;
    bool testCompleted = false;
    auto [sender, receiver] = util::Channel<int>::create(executor, 5);
    auto senderPtr = std::make_shared<decltype(sender)>(std::move(sender));

    util::spawn(
        executor,
        [&receiver, senderPtr = std::move(senderPtr), &testCompleted, &executor](
            boost::asio::yield_context
        ) mutable {
            // Start receiving - this will block because no data is sent
            auto receiveTask = [&receiver, &testCompleted](boost::asio::yield_context yield) {
                auto const value = receiver.asyncReceive(yield);
                EXPECT_FALSE(value.has_value());  // Should receive nullopt when sender is destroyed
                testCompleted = true;
            };

            util::spawn(executor, receiveTask);

            senderPtr.reset();
        }
    );

    executor.run_for(kTEST_TIMEOUT);
    EXPECT_TRUE(testCompleted);
}

// This test verifies the workaround for a bug in boost::asio::experimental::concurrent_channel
// where close() does not cancel pending async operations. Our Channel wrapper calls cancel() after
// close() to ensure pending operations are unblocked. See:
// https://github.com/chriskohlhoff/asio/issues/1575
TEST(ChannelTest, PendingAsyncSendsAreCancelledOnClose)
{
    boost::asio::thread_pool pool{4};
    static constexpr auto kPENDING_NUM_SENDERS = 10uz;

    // Channel with capacity 0 - all sends will block waiting for a receiver
    auto [sender, receiver] = util::Channel<int>::create(pool, 0);

    std::atomic<std::size_t> completedSends{0};
    std::counting_semaphore<kPENDING_NUM_SENDERS> semaphore{kPENDING_NUM_SENDERS};

    // Spawn multiple senders that will all block (no receiver is consuming)
    for (auto i = 0uz; i < kPENDING_NUM_SENDERS; ++i) {
        util::spawn(
            pool,
            [senderCopy = sender, i, &completedSends, &semaphore](
                boost::asio::yield_context yield
            ) mutable {
                semaphore.release(1);
                EXPECT_FALSE(senderCopy.asyncSend(static_cast<int>(i), yield));
                ++completedSends;
            }
        );
    }

    semaphore.acquire();

    // Close the channel by destroying the only receiver we have.
    // Our workaround calls cancel() after close() to unblock pending operations
    {
        [[maybe_unused]] auto r = std::move(receiver);
    }

    // All senders should complete (unblocked by our cancel() workaround)
    pool.join();

    // All sends should have completed (returned false due to closed channel)
    EXPECT_EQ(completedSends, kPENDING_NUM_SENDERS);
}

INSTANTIATE_CHANNEL_FOR_CLANG(int);
