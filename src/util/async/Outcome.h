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

#include <util/async/Concepts.h>
#include <util/async/context/impl/Cancellation.h>

#include <future>

namespace util::async {

template <typename RetType>
class BasicOperation;

template <typename RetType, typename StopSourceType>
class CancellableOperation;

template <typename RetType>
class BasicOutcome {
protected:
    std::promise<RetType> promise_;

public:
    using DataType = RetType;

    BasicOutcome() = default;
    BasicOutcome(BasicOutcome&&) = default;
    BasicOutcome(BasicOutcome const&) = delete;

    void
    setValue(std::convertible_to<RetType> auto&& val)
    {
        promise_.set_value(std::forward<decltype(val)>(val));
    }

    void
    setValue()
    {
        promise_.set_value({});
    }

    std::future<RetType>
    getStdFuture()
    {
        return promise_.get_future();
    }
};

template <typename RetType>
class Outcome : public BasicOutcome<RetType> {
public:
    BasicOperation<Outcome>
    getOperation()
    {
        return BasicOperation<Outcome>{this};
    }
};

template <typename RetType, typename StopSourceType>
class CancellableOutcome : public BasicOutcome<RetType> {
private:
    StopSourceType stopSource_;

public:
    CancellableOperation<RetType, StopSourceType>
    getOperation()
    {
        return CancellableOperation<RetType, StopSourceType>{this};
    }

    StopSourceType&
    getStopSource()
    {
        return stopSource_;
    }
};

}  // namespace util::async
