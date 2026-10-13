#pragma once

#include "util/Concepts.hpp"

#include <atomic>
#include <memory>

namespace util {

/**
 * @brief Atomic wrapper for integral and floating point types
 */
template <SomeNumberType NumberType>
class Atomic {
public:
    using ValueType = NumberType;

    Atomic() = default;

    /**
     * @brief Construct a new Atomic object
     *
     * @param value The initial value
     */
    Atomic(ValueType const value) : value_(value)
    {
    }

    ~Atomic() = default;

    // Copy and move constructors and assignment operators are not allowed for atomics
    Atomic(Atomic const&) = delete;
    Atomic(Atomic&&) = delete;
    Atomic&
    operator=(Atomic const&) = delete;
    Atomic&
    operator=(Atomic&&) = delete;

    /**
     * @brief Add a value to the current value
     *
     * @param value The value to add
     */
    void
    add(ValueType const value)
    {
        value_.fetch_add(value);
    }

    /**
     * @brief Update the current value to the new value
     *
     * @param value The new value
     */
    void
    set(ValueType const value)
    {
        value_ = value;
    }

    /**
     * @brief Get the current value
     *
     * @return The current value
     */
    [[nodiscard]] ValueType
    value() const
    {
        return value_;
    }

private:
    std::atomic<ValueType> value_{0};
};

template <SomeNumberType NumberType>
using AtomicPtr = std::unique_ptr<Atomic<NumberType>>;

}  // namespace util
