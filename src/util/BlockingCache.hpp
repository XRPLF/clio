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
#include <concepts>
#include <expected>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <utility>
#include <variant>

namespace util {

template <typename ValueType, typename ErrorType>
    requires(not std::same_as<ValueType, ErrorType>)
class BlockingCache {
    enum class State { Empty, Updating, Full };

    std::atomic<State> state_{State::Empty};
    util::Mutex<std::optional<ValueType>, std::shared_mutex> value_;
    boost::signals2::signal<void(std::expected<ValueType, ErrorType>)> updateFinished_;

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
    asyncGet(boost::asio::yield_context yield)
    {
        switch (state_) {
            case State::Updating: {
                return Result{wait(yield)};
            }
            case State::Full: {
                auto const value = value_.template lock<std::shared_lock>();
                ASSERT(value->has_value(), "Value should be presented when the cache is full");
                return Result{value};
            }
            case State::Empty: {
                return Result{std::nullopt};
            }
        };
    }

    template <typename Updater>
        requires(std::invocable<Updater, boost::asio::yield_context> and std::same_as<std::invoke_result_t<Updater, boost::asio::yield_context>, std::expected<std::pair<ValueType, bool>, ErrorType>>)
    std::expected<ValueType, ErrorType>
    update(Updater&& updater, boost::asio::yield_context yield)
    {
        if (state_ == State::Updating) {
            auto result = asyncGet(yield);
            return std::move(result).valueOrError();
        }
        state_ = State::Updating;
        auto result = updater(yield);
        if (result.hasValue()) {
            auto const [value, shouldBeCached] = std::move(result).value().first;
            if (shouldBeCached) {
                value_.lock().get() = std::move(value);
                state_ = State::Full;
            } else {
                value_.lock().get() = std::nullopt;
                state_ = State::Empty;
            }
            updateFinished_(value);
            return value;
        }
        return std::unexpected{std::move(result).error()};
    }

private:
    std::expected<ValueType, ErrorType>
    wait(boost::asio::yield_context yield)
    {
        boost::asio::steady_timer timer{yield.get_executor(), boost::asio::steady_timer::duration::max()};
        boost::system::error_code errorCode;

        std::expected<ValueType, ErrorType> result;
        boost::signals2::scoped_connection slot =
            updateFinished_.connect([yield, &timer, &result](std::expected<ValueType, ErrorType> value) {
                boost::asio::spawn(yield, [&timer, &result, value = std::move(value)](auto&&) {
                    result = std::move(value);
                    timer.cancel();
                });
            });

        if (state_ == State::Updating) {
            timer.async_wait(yield[errorCode]);
            return result;
        }
        return asyncGet(yield).valueOrError();
    }
};

template <typename ValueType, typename ErrorType>
    requires(not std::same_as<ValueType, ErrorType>)
class BlockingCache<ValueType, ErrorType>::Result {
    std::optional<std::expected<ValueType, ErrorType>> value_;

public:
    Result(std::optional<std::expected<ValueType, ErrorType>> value) : value_(std::move(value))
    {
    }

    bool
    hasValue() const
    {
        return std::holds_alternative<std::expected<ValueType, ErrorType>>(value_);
    }

    std::expected<ValueType, ErrorType>
    valueOrError() &&
    {
        ASSERT(value_.has_value(), "Value should be presented");
        return std::move(value_).value();
    }
};

}  // namespace util
