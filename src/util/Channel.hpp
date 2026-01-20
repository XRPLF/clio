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

#pragma once

#include "util/async/Concepts.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/detail/error_code.hpp>

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace util {

#ifdef __clang__
namespace detail {
// Forward declaration for compile-time check
template <typename T>
struct ChannelInstantiated;
}  // namespace detail
#endif

/**
 * @brief Specifies the producer concurrency model for a Channel.
 */
enum class ProducerType {
    Single, /**< Only one Sender can exist (non-copyable). Uses direct Guard ownership for zero overhead. */
    Multi   /**< Multiple Senders can exist (copyable). Uses shared_ptr<Guard> for shared ownership. */
};

/**
 * @brief Specifies the consumer concurrency model for a Channel.
 */
enum class ConsumerType {
    Single, /**< Only one Receiver can exist (non-copyable). Uses direct Guard ownership for zero overhead. */
    Multi   /**< Multiple Receivers can exist (copyable). Uses shared_ptr<Guard> for shared ownership. */
};

/**
 * @brief Represents a go-like channel, a multi-producer (Sender) multi-consumer (Receiver) thread-safe data pipe.
 * @note Use INSTANTIATE_CHANNEL_FOR_CLANG macro when using this class. See docs at the bottom of the file for more
 * details.
 *
 * @tparam T The type of data the channel transfers
 * @tparam P ProducerType::Multi (default) for multi-producer or ProducerType::Single for single-producer
 * @tparam C ConsumerType::Multi (default) for multi-consumer or ConsumerType::Single for single-consumer
 */
template <typename T, ProducerType P = ProducerType::Multi, ConsumerType C = ConsumerType::Multi>
class Channel {
    static constexpr bool kIS_MULTI_PRODUCER = (P == ProducerType::Multi);
    static constexpr bool kIS_MULTI_CONSUMER = (C == ConsumerType::Multi);

private:
    class ControlBlock {
        using InternalChannelType = boost::asio::experimental::concurrent_channel<void(boost::system::error_code, T)>;
        boost::asio::any_io_executor executor_;
        InternalChannelType ch_;

    public:
        template <typename ContextType>
            requires(not async::SomeExecutionContext<ContextType>)
        ControlBlock(ContextType&& context, std::size_t capacity)
            : executor_(context.get_executor()), ch_(context, capacity)
        {
        }

        template <async::SomeExecutionContext ContextType>
        ControlBlock(ContextType&& context, std::size_t capacity)
            : executor_(context.getExecutor().get_executor()), ch_(context.getExecutor(), capacity)
        {
        }

        [[nodiscard]] InternalChannelType&
        channel()
        {
            return ch_;
        }

        void
        close()
        {
            if (not isClosed()) {
                ch_.close();
                // Workaround for Boost bug: close() alone doesn't cancel pending async operations.
                // We must call cancel() to unblock them. The bug also causes cancel() to return
                // error_code 0 instead of channel_cancelled, so async operations must check
                // isClosed() to detect this case.
                // https://github.com/chriskohlhoff/asio/issues/1575
                ch_.cancel();
            }
        }

        [[nodiscard]] bool
        isClosed() const
        {
            return not ch_.is_open();
        }
    };

    /**
     * @brief This is used to close the channel once either all Senders or all Receivers are destroyed
     */
    struct Guard {
        std::shared_ptr<ControlBlock> shared;

        ~Guard()
        {
            shared->close();
        }
    };

public:
    /**
     * @brief The sending end of a channel.
     *
     * Sender is movable. For multi-producer channels, Sender is also copyable.
     * The channel remains open as long as at least one Sender exists.
     * When all Sender instances are destroyed, the channel is closed and receivers will receive std::nullopt.
     */
    class Sender {
        std::shared_ptr<ControlBlock> shared_;
        std::conditional_t<kIS_MULTI_PRODUCER, std::shared_ptr<Guard>, Guard> guard_;

        friend class Channel<T, P, C>;

        /**
         * @brief Constructs a Sender from a shared control block.
         * @param shared The shared control block managing the channel state
         */
        Sender(std::shared_ptr<ControlBlock> shared)
            : shared_(shared), guard_([shared = std::move(shared)]() {
                if constexpr (kIS_MULTI_PRODUCER) {
                    return std::make_shared<Guard>(std::move(shared));
                } else {
                    return Guard{std::move(shared)};
                }
            }())
        {
        }

    public:
        Sender(Sender&&) = default;
        Sender(Sender const&)
            requires kIS_MULTI_PRODUCER
        = default;
        Sender(Sender const&)
            requires(!kIS_MULTI_PRODUCER)
        = delete;

        Sender&
        operator=(Sender&&) = default;
        Sender&
        operator=(Sender const&)
            requires kIS_MULTI_PRODUCER
        = default;
        Sender&
        operator=(Sender const&)
            requires(!kIS_MULTI_PRODUCER)
        = delete;

        /**
         * @brief Asynchronously sends data through the channel using a coroutine.
         *
         * Blocks the coroutine until the data is sent or the channel is closed.
         *
         * @tparam D The type of data to send (must be convertible to T)
         * @param data The data to send
         * @param yield The Boost.Asio yield context for coroutine suspension
         * @return true if the data was sent successfully, false if the channel is closed
         */
        template <typename D>
        bool
        asyncSend(D&& data, boost::asio::yield_context yield)
            requires(std::convertible_to<std::remove_cvref_t<D>, std::remove_cvref_t<T>>)
        {
            boost::system::error_code const ecIn;
            boost::system::error_code ecOut;
            shared_->channel().async_send(ecIn, std::forward<D>(data), yield[ecOut]);

            // Workaround: asio channels bug returns ec=0 on cancel, check isClosed() instead
            if (not ecOut and shared_->isClosed())
                return false;

            return not ecOut;
        }

        /**
         * @brief Asynchronously sends data through the channel using a callback.
         *
         * The callback is invoked when the send operation completes.
         *
         * @tparam D The type of data to send (must be convertible to T)
         * @param data The data to send
         * @param fn Callback function invoked with true if successful, false if the channel is closed
         */
        template <typename D>
        void
        asyncSend(D&& data, std::invocable<bool> auto&& fn)
            requires(std::convertible_to<std::remove_cvref_t<D>, std::remove_cvref_t<T>>)
        {
            boost::system::error_code const ecIn;
            shared_->channel().async_send(
                ecIn,
                std::forward<D>(data),
                [fn = std::forward<decltype(fn)>(fn), shared = shared_](boost::system::error_code ec) mutable {
                    // Workaround: asio channels bug returns ec=0 on cancel, check isClosed() instead
                    if (not ec and shared->isClosed()) {
                        fn(false);
                        return;
                    }

                    fn(not ec);
                }
            );
        }

        /**
         * @brief Attempts to send data through the channel without blocking.
         *
         * @tparam D The type of data to send (must be convertible to T)
         * @param data The data to send
         * @return true if the data was sent successfully, false if the channel is full or closed
         */
        template <typename D>
        bool
        trySend(D&& data)
            requires(std::convertible_to<std::remove_cvref_t<D>, std::remove_cvref_t<T>>)
        {
            boost::system::error_code ec;
            return shared_->channel().try_send(ec, std::forward<D>(data));
        }
    };

    /**
     * @brief The receiving end of a channel.
     *
     * Receiver is movable. For multi-consumer channels, Receiver is also copyable.
     * Multiple receivers can consume from the same multi-consumer channel concurrently.
     * When all Receiver instances are destroyed, the channel is closed and senders will fail to send.
     */
    class Receiver {
        std::shared_ptr<ControlBlock> shared_;
        std::conditional_t<kIS_MULTI_CONSUMER, std::shared_ptr<Guard>, Guard> guard_;

        friend class Channel<T, P, C>;

        /**
         * @brief Constructs a Receiver from a shared control block.
         * @param shared The shared control block managing the channel state
         */
        Receiver(std::shared_ptr<ControlBlock> shared)
            : shared_(shared), guard_([shared = std::move(shared)]() {
                if constexpr (kIS_MULTI_CONSUMER) {
                    return std::make_shared<Guard>(std::move(shared));
                } else {
                    return Guard{std::move(shared)};
                }
            }())
        {
        }

    public:
        Receiver(Receiver&&) = default;
        Receiver(Receiver const&)
            requires kIS_MULTI_CONSUMER
        = default;
        Receiver(Receiver const&)
            requires(!kIS_MULTI_CONSUMER)
        = delete;

        Receiver&
        operator=(Receiver&&) = default;
        Receiver&
        operator=(Receiver const&)
            requires kIS_MULTI_CONSUMER
        = default;
        Receiver&
        operator=(Receiver const&)
            requires(!kIS_MULTI_CONSUMER)
        = delete;

        /**
         * @brief Attempts to receive data from the channel without blocking.
         *
         * @return std::optional containing the received value, or std::nullopt if the channel is empty or closed
         */
        std::optional<T>
        tryReceive()
        {
            std::optional<T> result;
            shared_->channel().try_receive([&result](boost::system::error_code ec, auto&& value) {
                if (not ec)
                    result = std::forward<decltype(value)>(value);
            });

            return result;
        }

        /**
         * @brief Asynchronously receives data from the channel using a coroutine.
         *
         * Blocks the coroutine until data is available or the channel is closed.
         *
         * @param yield The Boost.Asio yield context for coroutine suspension
         * @return std::optional containing the received value, or std::nullopt if the channel is closed
         */
        [[nodiscard]] std::optional<T>
        asyncReceive(boost::asio::yield_context yield)
        {
            boost::system::error_code ec;
            auto value = shared_->channel().async_receive(yield[ec]);

            if (ec)
                return std::nullopt;

            return value;
        }

        /**
         * @brief Asynchronously receives data from the channel using a callback.
         *
         * The callback is invoked when data is available or the channel is closed.
         *
         * @param fn Callback function invoked with std::optional containing the value, or std::nullopt if closed
         */
        void
        asyncReceive(std::invocable<std::optional<std::remove_cvref_t<T>>> auto&& fn)
        {
            shared_->channel().async_receive(
                [fn = std::forward<decltype(fn)>(fn)](boost::system::error_code ec, T&& value) mutable {
                    if (ec) {
                        fn(std::optional<T>(std::nullopt));
                        return;
                    }

                    fn(std::make_optional<T>(std::move(value)));
                }
            );
        }

        /**
         * @brief Checks if the channel is closed.
         *
         * A channel is closed when all Sender instances have been destroyed.
         *
         * @return true if the channel is closed, false otherwise
         */
        [[nodiscard]] bool
        isClosed() const
        {
            return shared_->isClosed();
        }
    };

    /**
     * @brief Factory function to create channel components.
     * @param context A supported context type (either io_context or thread_pool)
     * @param capacity Size of the internal buffer on the channel
     * @return A pair of Sender and Receiver
     */
    static std::pair<Sender, Receiver>
    create(auto&& context, std::size_t capacity)
    {
#ifdef __clang__
        static_assert(
            util::detail::ChannelInstantiated<T>::value,
            "When using Channel<T> with Clang, you must add INSTANTIATE_CHANNEL_FOR_CLANG(T) "
            "to one .cpp file. See documentation at the bottom of Channel.hpp for details."
        );
#endif
        auto shared = std::make_shared<ControlBlock>(std::forward<decltype(context)>(context), capacity);
        auto sender = Sender{shared};
        auto receiver = Receiver{std::move(shared)};

        return {std::move(sender), std::move(receiver)};
    }
};

}  // namespace util

// ================================================================================================
// Clang/Apple Clang Workaround for Boost.Asio Experimental Channels
// ================================================================================================
//
// IMPORTANT: When using Channel<T> with Clang or Apple Clang, you MUST add the following line
// to ONE .cpp file that uses Channel<T>:
//
//     INSTANTIATE_CHANNEL_FOR_CLANG(YourType)
//
// Example:
//     // In ChannelTests.cpp or any .cpp file that uses Channel<int>:
//     #include "util/Channel.hpp"
//     INSTANTIATE_CHANNEL_FOR_CLANG(int)
//
// Why this is needed:
// Boost.Asio's experimental concurrent_channel has a bug where close() doesn't properly cancel
// pending async operations. When using cancellation signals (which we do in our workaround),
// Clang generates vtable references for internal cancellation_handler types but Boost.Asio
// doesn't provide the definitions, causing linker errors:
//
//   Undefined symbols for architecture arm64:
//     "boost::asio::detail::cancellation_handler<...>::call(boost::asio::cancellation_type)"
//     "boost::asio::detail::cancellation_handler<...>::destroy()"
//
// This macro explicitly instantiates the required template specializations.
//
// See: https://github.com/chriskohlhoff/asio/issues/1575
//
#ifdef __clang__

#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/channel_traits.hpp>
#include <boost/asio/experimental/detail/channel_service.hpp>

namespace util::detail {
// Tag type used to verify that INSTANTIATE_CHANNEL_FOR_CLANG was called for a given type
template <typename T>
struct ChannelInstantiated : std::false_type {};
}  // namespace util::detail

#define INSTANTIATE_CHANNEL_FOR_CLANG(T)                                                                       \
    /* NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor) */                                           \
    template class boost::asio::detail::cancellation_handler<                                                  \
        boost::asio::experimental::detail::channel_service<boost::asio::detail::posix_mutex>::                 \
            op_cancellation<boost::asio::experimental::channel_traits<>, void(boost::system::error_code, T)>>; \
    namespace util::detail {                                                                                   \
    template <>                                                                                                \
    struct ChannelInstantiated<T> : std::true_type {};                                                         \
    }

#else

// No workaround needed for non-Clang compilers
#define INSTANTIATE_CHANNEL_FOR_CLANG(T)

#endif
