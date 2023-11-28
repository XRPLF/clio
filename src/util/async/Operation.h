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

#include <fmt/std.h>
#include <util/async/Concepts.h>
#include <util/async/Outcome.h>
#include <util/async/context/impl/Cancellation.h>

#include <future>

namespace util::async {

template <typename OutcomeType>
class BasicOperation {
protected:
    OutcomeType* outcome_ = nullptr;
    std::future<typename OutcomeType::DataType> future_;

public:
    using DataType = typename OutcomeType::DataType;

    explicit BasicOperation(OutcomeType* outcome) : outcome_{outcome}, future_{outcome->getStdFuture()}
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
class CancellableOperation : public BasicOperation<CancellableOutcome<RetType, StopSourceType>> {
    using OutcomeType = CancellableOutcome<RetType, StopSourceType>;

public:
    explicit CancellableOperation(OutcomeType* outcome) : BasicOperation<OutcomeType>(outcome)
    {
    }

    void
    requestStop()
    {
        this->outcome_->getStopSource().requestStop();
    }
};

template <typename T>
using Operation = BasicOperation<Outcome<T>>;

}  // namespace util::async
