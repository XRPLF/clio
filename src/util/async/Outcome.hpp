#pragma once

#include "util/async/context/impl/Cancellation.hpp"

#include <concepts>
#include <future>

namespace util::async {

template <typename RetType, typename StopSourceType>
class StoppableOperation;

namespace impl {

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

    /**
     * @brief Sets the value on the inner `promise`
     *
     * @param val The value to set
     */
    void
    setValue(std::convertible_to<RetType> auto&& val)
    {
        promise_.set_value(std::forward<decltype(val)>(val));
    }

    /**
     * @brief Sets the value channel for void operations
     */
    void
    setValue()
    {
        promise_.set_value({});
    }

    /**
     * @brief Get the `future` for the inner `promise`
     *
     * @return The standard future matching the inner `promise`
     */
    [[nodiscard]] std::future<RetType>
    getStdFuture()
    {
        return promise_.get_future();
    }
};

}  // namespace impl

/**
 * @brief Unstoppable outcome
 *
 * @tparam RetType The return type of the operation.
 */
template <typename RetType>
class Outcome : public impl::BasicOutcome<RetType> {
public:
    /**
     * @brief Gets the unstoppable operation for this outcome
     *
     * @return An unstoppable operation for this outcome
     */
    [[nodiscard]] impl::BasicOperation<Outcome>
    getOperation()
    {
        return impl::BasicOperation<Outcome>{this};
    }
};

/**
 * @brief Stoppable outcome
 *
 * @tparam RetType The return type of the operation.
 * @tparam StopSourceType The type of the stop source.
 */
template <typename RetType, typename StopSourceType>
class StoppableOutcome : public impl::BasicOutcome<RetType> {
private:
    StopSourceType stopSource_;

public:
    /**
     * @brief Gets the stoppable operation for this outcome
     *
     * @return A stoppable operation for this outcome
     */
    [[nodiscard]] StoppableOperation<RetType, StopSourceType>
    getOperation()
    {
        return StoppableOperation<RetType, StopSourceType>{this};
    }

    /**
     * @brief Gets the stop source for this outcome
     *
     * @return The stop source
     */
    [[nodiscard]] StopSourceType&
    getStopSource()
    {
        return stopSource_;
    }
};

}  // namespace util::async
