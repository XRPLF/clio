//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "util/async/Concepts.h"

#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

namespace util::async::detail {

class StopState {
    std::atomic_bool isStopRequested_{false};

public:
    void
    requestStop()
    {
        isStopRequested_ = true;
    }

    bool
    isStopRequested() const
    {
        return isStopRequested_;
    }
};

using SharedStopState = std::shared_ptr<StopState>;

class YieldContextStopSource {
    SharedStopState shared_ = std::make_shared<StopState>();

public:
    class Token {
        friend class YieldContextStopSource;
        SharedStopState shared_;
        boost::asio::yield_context yield_;

        Token(YieldContextStopSource* source, boost::asio::yield_context yield)
            : shared_{source->shared_}, yield_{std::move(yield)}
        {
        }

    public:
        bool
        isStopRequested() const
        {
            // yield explicitly
            boost::asio::post(yield_);
            return shared_->isStopRequested();
        }

        operator bool() const
        {
            return isStopRequested();
        }
    };

    Token
    operator[](boost::asio::yield_context yield)
    {
        return {this, yield};
    }

    void
    requestStop()
    {
        shared_->requestStop();
    }
};

class BasicStopSource {
    SharedStopState shared_ = std::make_shared<StopState>();

public:
    class Token {
        friend class BasicStopSource;
        SharedStopState shared_;

        Token(BasicStopSource* source) : shared_{source->shared_}
        {
        }

    public:
        Token(Token const&) = default;
        Token(Token&&) = default;

        bool
        isStopRequested() const
        {
            return shared_->isStopRequested();
        }

        operator bool() const
        {
            return isStopRequested();
        }
    };

    Token
    getToken()
    {
        return {this};
    }

    void
    requestStop()
    {
        shared_->requestStop();
    }
};

[[nodiscard]] inline auto
getTimoutHandleIfNeeded(
    SomeExecutionContext auto& ctx,
    SomeOptStdDuration auto timeout,
    SomeStopSource auto& stopSource
)
{
    using TimerType = typename std::decay_t<decltype(ctx)>::Timer;
    std::optional<TimerType> timer;
    if (timeout) {
        timer.emplace(TimerType(ctx, *timeout, [&stopSource](auto cancelled) {
            if (not cancelled)
                stopSource.requestStop();
        }));
    }
    return timer;
}

}  // namespace util::async::detail