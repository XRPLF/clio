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

#include "util/Concepts.h"

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
            // compare_exchange_weak returns false if the value is not exchanged and updates the current value
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
     * @return ValueType The current value
     */
    ValueType
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
