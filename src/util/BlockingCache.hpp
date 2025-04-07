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
#include <functional>
#include <optional>
#include <shared_mutex>
#include <utility>

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

    using Updater = std::function<std::expected<ValueType, ErrorType>(boost::asio::yield_context)>;
    using Verifier = std::function<bool(ValueType const&)>;

    [[nodiscard]] std::expected<ValueType, ErrorType>
    asyncGet(boost::asio::yield_context yield, Updater updater, Verifier verifier)
    {
        switch (state_) {
            case State::Updating: {
                return wait(yield);
            }
            case State::Full: {
                auto const value = value_.template lock<std::shared_lock>();
                ASSERT(value->has_value(), "Value should be presented when the cache is full");
                return value;
            }
            case State::Empty: {
                return update(yield, std::move(updater), std::move(verifier));
            }
        };
    }

    std::expected<ValueType, ErrorType>
    update(boost::asio::yield_context yield, Updater updater, Verifier verifier)
    {
        if (state_ == State::Updating) {
            return asyncGet(yield);
        }
        state_ = State::Updating;

        auto const result = updater(yield);
        auto const shouldBeCached = result.has_value() and verifier(result.value());

        if (shouldBeCached) {
            value_.lock().get() = result.value();
            state_ = State::Full;
        } else {
            state_ = State::Empty;
            value_.lock().get() = std::nullopt;
        }

        updateFinished_(result);
        return result;
    }

    void
    invalidate()
    {
        if (state_ == State::Full) {
            state_ = State::Empty;
            value_.lock().get() = std::nullopt;
        }
    }

private:
    std::expected<ValueType, ErrorType>
    wait(boost::asio::yield_context yield)
    {
        boost::asio::steady_timer timer{yield.get_executor(), boost::asio::steady_timer::duration::max()};
        boost::system::error_code errorCode;

        std::optional<std::expected<ValueType, ErrorType>> result;
        boost::signals2::scoped_connection slot =
            updateFinished_.connect([yield, &timer, &result](std::expected<ValueType, ErrorType> value) {
                boost::asio::spawn(yield, [&timer, &result, value = std::move(value)](auto&&) {
                    result = std::move(value);
                    timer.cancel();
                });
            });

        if (state_ == State::Updating) {
            timer.async_wait(yield[errorCode]);
            ASSERT(result.has_value(), "There should be some value after waiting");
            return std::move(result).value();
        }
        return asyncGet(yield).valueOrError();
    }
};

}  // namespace util
