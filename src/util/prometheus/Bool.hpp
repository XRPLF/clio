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

#include "util/Assert.hpp"
#include "util/prometheus/Gauge.hpp"

#include <cstdint>
#include <functional>

namespace util::prometheus {

template <typename T>
concept SomeBoolImpl = requires(T a) {
    { a.set(0) } -> std::same_as<void>;
    { a.value() } -> std::same_as<int64_t>;
};

/**
 * @brief A wrapped to provide bool interface for a Prometheus metric
 * @note Prometheus does not have a native bool type, so we use a counter with a value of 0 or 1
 */
template <SomeBoolImpl ImplType>
class AnyBool {
    std::reference_wrapper<ImplType> impl_;

public:
    /**
     * @brief Construct a bool metric
     *
     * @param impl The implementation of the metric
     */
    explicit AnyBool(ImplType& impl) : impl_(impl)
    {
    }

    /**
     * @brief Set the value of the bool metric
     *
     * @param value The value to set
     * @return A reference to the metric
     */
    AnyBool&
    operator=(bool value)
    {
        impl_.get().set(value ? 1 : 0);
        return *this;
    }

    /**
     * @brief Get the value of the bool metric
     *
     * @return The value of the metric
     */
    operator bool() const
    {
        auto const value = impl_.get().value();
        ASSERT(value == 0 || value == 1, "Invalid value for bool: {}", value);
        return value == 1;
    }
};

/**
 * @brief Alias for Prometheus bool metric with GaugeInt implementation
 */
using Bool = AnyBool<GaugeInt>;

}  // namespace util::prometheus
