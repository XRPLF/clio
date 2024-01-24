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
#include "util/async/Outcome.h"
#include "util/async/context/impl/Cancellation.h"
#include "util/async/context/impl/Timer.h"

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <optional>

namespace util::async {

template <typename OutcomeType>
class BasicOperation {
protected:
    std::future<typename OutcomeType::DataType> future_;

public:
    using DataType = typename OutcomeType::DataType;

    explicit BasicOperation(OutcomeType* outcome) : future_{outcome->getStdFuture()}
    {
    }

    BasicOperation(BasicOperation&&) = default;
    BasicOperation(BasicOperation const&) = delete;

    auto
    get() -> decltype(auto)
    {
        return future_.get();
    }

    void
    wait()
    {
        future_.wait();
    }
};

template <typename RetType, typename StopSourceType>
class StoppableOperation : public BasicOperation<StoppableOutcome<RetType, StopSourceType>> {
    using OutcomeType = StoppableOutcome<RetType, StopSourceType>;

    StopSourceType stopSource_;

public:
    explicit StoppableOperation(OutcomeType* outcome)
        : BasicOperation<OutcomeType>(outcome), stopSource_(outcome->getStopSource())
    {
    }

    void
    requestStop()
    {
        stopSource_.requestStop();
    }
};

template <typename CtxType, typename OpType>
struct BasicScheduledOperation {
    struct State {
        std::mutex m_;
        std::condition_variable ready_;
        std::optional<OpType> op_{std::nullopt};

        void
        emplace(auto&& op)
        {
            std::lock_guard lock{m_};
            op_.emplace(std::forward<decltype(op)>(op));
            ready_.notify_all();
        }

        OpType&
        get()
        {
            std::unique_lock lock{m_};
            ready_.wait(lock, [this] { return op_.has_value(); });
            return op_.value();
        }
    };

    std::shared_ptr<State> state_ = std::make_shared<State>();
    typename CtxType::Timer timer_;

    BasicScheduledOperation(auto& executor, auto delay, auto&& fn)
        : timer_(executor, delay, [state = state_, fn = std::forward<decltype(fn)>(fn)](auto ec) mutable {
            state->emplace(fn(ec));
        })
    {
    }

    auto
    get() -> decltype(auto)
    {
        return state_->get().get();
    }

    void
    wait()
    {
        state_->get().wait();
    }

    void
    cancel()
    {
        timer_.cancel();
    }

    void
    requestStop()
        requires(SomeStoppableOperation<OpType>)
    {
        state_->get().requestStop();
    }
};

template <typename T>
using Operation = BasicOperation<Outcome<T>>;

template <typename CtxType, typename OpType>
using ScheduledOperation = BasicScheduledOperation<CtxType, OpType>;

}  // namespace util::async
