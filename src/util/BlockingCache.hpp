#pragma once

#include "util/Assert.hpp"
#include "util/Mutex.hpp"
#include "util/Spawn.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <concepts>
#include <expected>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <utility>

namespace util {

/**
 * @brief A thread-safe cache that blocks getting operations until the cache is updated
 *
 * @tparam ValueType The type of value to be cached
 * @tparam ErrorType The type of error that can occur during updates
 */
template <typename ValueType, typename ErrorType>
    requires(not std::same_as<ValueType, ErrorType>)
class BlockingCache {
public:
    /**
     * @brief Possible states of the cache
     */
    enum class State { NoValue, Updating, HasValue };

private:
    std::atomic<State> state_{State::NoValue};
    util::Mutex<std::optional<ValueType>, std::shared_mutex> value_;
    boost::signals2::signal<void(std::expected<ValueType, ErrorType>)> updateFinished_;

public:
    /**
     * @brief Default constructor - creates an empty cache
     */
    BlockingCache() = default;

    /**
     * @brief Construct a cache with an initial value
     * @param initialValue The value to initialize the cache with
     */
    explicit BlockingCache(ValueType initialValue)
        : state_{State::HasValue}, value_(std::move(initialValue))
    {
    }

    BlockingCache(BlockingCache&&) = delete;
    BlockingCache(BlockingCache const&) = delete;
    BlockingCache&
    operator=(BlockingCache&&) = delete;
    BlockingCache&
    operator=(BlockingCache const&) = delete;

    /**
     * @brief Function type for cache update operations
     * @details Called when the cache needs to be populated or refreshed
     */
    using Updater = std::function<std::expected<ValueType, ErrorType>(boost::asio::yield_context)>;

    /**
     * @brief Function type to verify if a value should be cached
     * @details Returns true if the value should be stored in the cache
     */
    using Verifier = std::function<bool(ValueType const&)>;

    /**
     * @brief Asynchronously get a value from the cache, updating if necessary
     *
     * @param yield The asio yield context for coroutine suspension
     * @param updater Function to generate a new value if needed
     * @param verifier Function to validate whether a value should be cached
     * @return std::expected<ValueType, ErrorType> The cached value or an error
     *
     * Depending on the current cache state, this will either:
     * - Return the cached value if it's already present
     * - Wait for an ongoing update to complete
     * - Trigger a new update if the cache is empty
     */
    [[nodiscard]] std::expected<ValueType, ErrorType>
    asyncGet(boost::asio::yield_context yield, Updater updater, Verifier verifier)
    {
        switch (state_) {
            case State::Updating: {
                return wait(yield, std::move(updater), std::move(verifier));
            }
            case State::HasValue: {
                auto const value = value_.template lock<std::shared_lock>();
                ASSERT(value->has_value(), "Value should be presented when the cache is full");
                return **value;  // NOLINT(bugprone-unchecked-optional-access)
            }
            case State::NoValue: {
                return update(yield, std::move(updater), std::move(verifier));
            }
        };
        std::unreachable();
    }

    /**
     * @brief Force an update of the cache value
     *
     * @param yield The ASIO yield context for coroutine suspension
     * @param updater Function to generate a new value
     * @param verifier Function to validate whether a value should be cached
     * @return std::expected<ValueType, ErrorType> The new value or an error
     *
     * Initiates a cache update operation regardless of current state.
     * If another update is already in progress, waits for it to complete.
     */
    [[nodiscard]] std::expected<ValueType, ErrorType>
    update(boost::asio::yield_context yield, Updater updater, Verifier verifier)
    {
        if (state_ == State::Updating) {
            return asyncGet(yield, std::move(updater), std::move(verifier));
        }
        state_ = State::Updating;

        auto const result = updater(yield);
        auto const shouldBeCached = result.has_value() and verifier(*result);

        if (shouldBeCached) {
            value_.lock().get() = *result;
            state_ = State::HasValue;
        } else {
            state_ = State::NoValue;
            value_.lock().get() = std::nullopt;
        }

        updateFinished_(result);
        return result;
    }

    /**
     * @brief Invalidates the currently cached value if present
     *
     * Clears the cache and sets its state to Empty.
     * Has no effect if the cache is already empty or being updated.
     */
    void
    invalidate()
    {
        if (state_ == State::HasValue) {
            state_ = State::NoValue;
            value_.lock().get() = std::nullopt;
        }
    }

    /**
     * @brief Returns the current state of the cache
     * @return Current cache state (Empty, Updating, or Full)
     */
    [[nodiscard]] State
    state() const
    {
        return state_;
    }

private:
    /**
     * @brief Wait for an ongoing update to complete
     *
     * @param yield The ASIO yield context for coroutine suspension
     * @param updater Function to generate a new value if needed
     * @param verifier Function to validate whether a value should be cached
     * @return std::expected<ValueType, ErrorType> The result of the ongoing update
     *
     * This method blocks the current coroutine until the ongoing update signals completion.
     */
    std::expected<ValueType, ErrorType>
    wait(boost::asio::yield_context yield, Updater updater, Verifier verifier)
    {
        struct SharedContext {
            SharedContext(boost::asio::yield_context y)
                : timer(y.get_executor(), boost::asio::steady_timer::duration::max())
            {
            }

            boost::asio::steady_timer timer;
            std::optional<std::expected<ValueType, ErrorType>> result;
        };

        auto sharedContext = std::make_shared<SharedContext>(yield);
        boost::system::error_code errorCode;

        boost::signals2::scoped_connection const slot =
            updateFinished_.connect([yield,
                                     sharedContext](std::expected<ValueType, ErrorType> value) {
                util::spawn(
                    yield,
                    [sharedContext = std::move(sharedContext), value = std::move(value)](auto&&) {
                        sharedContext->result = std::move(value);
                        sharedContext->timer.cancel();
                    }
                );
            });

        if (state_ == State::Updating) {
            sharedContext->timer.async_wait(yield[errorCode]);
            ASSERT(sharedContext->result.has_value(), "There should be some value after waiting");
            return *std::move(sharedContext->result);  // NOLINT(bugprone-unchecked-optional-access)
        }
        return asyncGet(yield, std::move(updater), std::move(verifier));
    }
};

}  // namespace util
