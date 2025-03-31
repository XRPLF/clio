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

#include "util/Mutex.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <expected>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>

namespace util {

// Helper concepts to constrain template parameters
template <typename F, typename ValueType>
concept VerifierConcept = requires(F f, std::optional<ValueType> const& v) {
    { f(v) } -> std::same_as<bool>;
};

template <typename F, typename ValueType>
concept UpdaterConcept = requires(F f, boost::asio::yield_context yield) {
    { f(yield) } -> std::same_as<std::expected<ValueType, std::string>>;
};

template <typename ValueType>
class BlockingCache2 {
    util::Mutex<std::optional<ValueType>, std::shared_mutex> value_;
    std::atomic_bool updating_{false};
    boost::signals2::signal<void()> updateFinished_;

public:
    template <VerifierConcept<ValueType> Verifier, UpdaterConcept<ValueType> Updater>
    std::expected<ValueType, std::string>
    asyncGet(
        boost::asio::yield_context yield,
        Verifier verifier,
        Updater updater,
        std::optional<std::chrono::steady_clock::duration> timeout
    )
    {
        {
            auto const value = value_.template lock<std::shared_lock>();
            if (verifier(value.get())) {
                return value.get();
            }
        }

        if (updating_.exchange(true)) {
            boost::asio::steady_timer timer{
                yield.get_executor(), timeout.value_or(boost::asio::steady_timer::duration::max())
            };
            boost::system::error_code errorCode;

            boost::signals2::scoped_connection slot = updateFinished_.connect([&timer]() { timer.cancel(); });
            if (updating_) {
                timer.async_wait(yield[errorCode]);
                if (errorCode != boost::asio::error::operation_aborted) {
                    return std::unexpected{"Waiting timeout"};
                }
            }
        } else {
            auto const expectedValue = updater(yield);
            if (not expectedValue.has_value()) {
                updateFinished_();
                updating_ = false;
                return std::unexpected{std::move(expectedValue).error()};
            }
            auto value = value_.template lock<std::unique_lock>();
            value.get() = std::move(expectedValue).value();
            updateFinished_();
            updating_ = false;
        }

        auto const value = value_.template lock<std::shared_lock>();
        if (!verifier(value.get())) {
            return std::unexpected{"Failed to update cache"};
        }
        return value.get();
    }
};

}  // namespace util
