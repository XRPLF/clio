//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/async/context/impl/Cancellation.h"

#include <future>

namespace util::async {

template <typename RetType, typename StopSourceType>
class StoppableOperation;

namespace detail {

template <typename RetType>
class BasicOperation;

/**
 * @brief Base for all `promise` side of async operations
 *
 * @tparam RetType The return type of the operation.
 */
template <typename RetType>
class BasicOutcome {
protected:
    std::promise<RetType> promise_;

public:
    using DataType = RetType;

    BasicOutcome() = default;
    BasicOutcome(BasicOutcome&&) = default;
    BasicOutcome(BasicOutcome const&) = delete;

    /** @brief Sets the value on the inner `promise` */
    void
    setValue(std::convertible_to<RetType> auto&& val)
    {
        promise_.set_value(std::forward<decltype(val)>(val));
    }

    /** @brief Sets the value channel for void operations */
    void
    setValue()
    {
        promise_.set_value({});
    }

    /** @brief Get the `future` for the inner `promise` */
    std::future<RetType>
    getStdFuture()
    {
        return promise_.get_future();
    }
};

}  // namespace detail

/**
 * @brief Unstoppable outcome
 *
 * @tparam RetType The return type of the operation.
 */
template <typename RetType>
class Outcome : public detail::BasicOutcome<RetType> {
public:
    /** @brief Gets the unstoppable operation for this outcome */
    detail::BasicOperation<Outcome>
    getOperation()
    {
        return detail::BasicOperation<Outcome>{this};
    }
};

/**
 * @brief Stoppable outcome
 *
 * @tparam RetType The return type of the operation.
 * @tparam StopSourceType The type of the stop source.
 */
template <typename RetType, typename StopSourceType>
class StoppableOutcome : public detail::BasicOutcome<RetType> {
private:
    StopSourceType stopSource_;

public:
    /** @brief Gets the stoppable operation for this outcome */
    StoppableOperation<RetType, StopSourceType>
    getOperation()
    {
        return StoppableOperation<RetType, StopSourceType>{this};
    }

    /** @brief Gets the stop source for this outcome */
    StopSourceType&
    getStopSource()
    {
        return stopSource_;
    }
};

}  // namespace util::async
