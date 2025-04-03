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

#include "util/Assert.hpp"
#include "util/Mutex.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <chrono>
#include <expected>
#include <optional>
#include <shared_mutex>
#include <string>
#include <utility>

namespace util {

/*
    cacheEntry = cache.get(yield, timeout);
    if (cacheEntry.has_error()) {
        return error;
    }
    updateObject = cache.update();
    result = forwardToRippled(...);
    if (result.has_value()) {
        updateObject.put(result.value());
        return value;
    } else {
        return result.error();
    }

}
*/

template <typename ValueType>
class BlockingCache {
public:
    enum class Error;

private:
    enum class State { Empty, Updating, Full };

    std::atomic<State> state_{State::Empty};
    util::Mutex<std::optional<ValueType>, std::shared_mutex> value_;
    boost::signals2::signal<void()> updateFinished_;
    boost::signals2::signal<void(Error)> updateFailed_;

public:
    BlockingCache() = default;
    BlockingCache(ValueType initialValue) : state_{State::Full}, value_(std::move(initialValue))
    {
    }

    BlockingCache(BlockingCache&&) = delete;
    BlockingCache(BlockingCache const&) = delete;
    BlockingCache&
    operator=(BlockingCache&&) = delete;
    BlockingCache&
    operator=(BlockingCache const&) = delete;

    class Result;

    Result
    asyncGet(boost::asio::yield_context yield, std::optional<std::chrono::steady_clock::duration> waitTimeout)
    {
        switch (state_) {
            case State::Updating: {
                if (auto waitResult = wait(yield, waitTimeout); not waitResult.has_value())
                    return std::unexpected{std::move(waitResult).error()};
                // else
                [[fallthrough]];
            }
            case State::Full: {
                auto const value = value_.template lock<std::shared_lock>();
                ASSERT(value->has_value(), "Value should be presented when the cache is full");
                return value;
            }
            case State::Empty: {
                return std::nullopt;
            }
        };
    }

    class Update;
    friend class Update;

    std::expected<Update, Error>
    update()
    {
        if (state_ != State::Updating)
            return Update{this};

        return std::unexpected{Error::AlreadyUpdating};
    }

private:
    std::expected<void, Error>
    wait(boost::asio::yield_context yield, std::optional<std::chrono::steady_clock::duration> timeout)
    {
        boost::asio::steady_timer timer{
            yield.get_executor(), timeout.value_or(boost::asio::steady_timer::duration::max())
        };
        boost::system::error_code errorCode;

        boost::signals2::scoped_connection finishSlot = updateFinished_.connect([&timer]() { timer.cancel(); });

        std::optional<std::string> updateError;
        boost::signals2::scoped_connection failureSlot =
            updateFinished_.connect([&updateError, &timer](std::string error) {
                updateError = std::move(error);
                timer.cancel();
            });

        if (state_ == State::Updating) {
            timer.async_wait(yield[errorCode]);
            if (errorCode != boost::asio::error::operation_aborted)
                return std::unexpected{Error::WaitingTimeout};

            if (updateError.has_value())
                return std::unexpected{std::move(updateError).value()};
        }
        return {};
    }
};

template <typename ValueType>
enum class BlockingCache<ValueType>::Error {
    WaitingTimeout,
    UpdateCancelled,
    AlreadyUpdating
};

template <typename ValueType>
class BlockingCache<ValueType>::Result {
    std::expected<std::optional<ValueType>, BlockingCache::Error> value_;

public:
    explicit Result(std::optional<ValueType> value) : value_(std::move(value))
    {
    }

    explicit Result(BlockingCache::Error error) : value_(std::unexpected{error})
    {
    }

    bool
    hasValue() const
    {
        return not hasError() && value_->has_value();
    }

    bool
    hasError() const
    {
        return value_.has_value();
    }

    ValueType const&
    value() const
    {
        ASSERT(hasValue(), "There must be a value to get");
        return value_->value();
    }

    ValueType
    value() &&
    {
        ASSERT(hasValue(), "There must be a value to get");
        return std::move(value_)->value();
    }

    BlockingCache::Error
    error()
    {
        ASSERT(hasError(), "There must be an error to get");
        return value_.error();
    }
};

template <typename ValueType>
class BlockingCache<ValueType>::Update {
    BlockingCache<ValueType>& cache_;
    bool wasUpdated_ = false;

public:
    ~Update()
    {
        if (not wasUpdated_) {
            cache_.state_ = BlockingCache::State::Empty;
            cache_.updateFailed_(BlockingCache::Error::UpdateCancelled);
        }
    }

    void
    put(ValueType value)
    {
        cache_.value_.lock().get() = std::move(value);
        cache_.state_ = BlockingCache::State::Full;
        cache_.updateFinished_();
        wasUpdated_ = true;
    }

private:
    friend class BlockingCache<ValueType>;

    Update(BlockingCache<ValueType> cache) : cache_(cache)
    {
        cache_->state_ = BlockingCache<ValueType>::State::Updating;
    }
};

}  // namespace util
