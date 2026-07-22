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
        if constexpr (std::is_integral_v<ValueType>) {
            value_.fetch_add(value);
        } else {
#if __cpp_lib_atomic_float >= 201711L
            value_.fetch_add(value);
#else
            // Workaround for atomic float not being supported by the standard library
            // compare_exchange_weak returns false if the value is not exchanged and updates the
            // current value
            auto current = value_.load();
            while (!value_.compare_exchange_weak(current, current + value)) {
            }
#endif
        }
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
