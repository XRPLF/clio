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

#include "util/Assert.hpp"
#include "util/prometheus/MetricBase.hpp"
#include "util/prometheus/OStream.hpp"
#include "util/prometheus/impl/AnyCounterBase.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace util::prometheus {

/**
 * @brief A Prometheus bool metric.
 * @note Prometheus does not have a native bool type, so we use a counter with a value of 0 or 1.
 */
class AnyBool : public MetricBase, impl::AnyCounterBase<uint8_t> {
public:
    /**
     * @brief Construct a bool metric.
     * @param name The name of the metric.
     * @param labelsString The labels string for the metric.
     * @param value The initial value of the metric.
     * @param impl The implementation of the metric.
     */
    template <impl::SomeCounterImpl ImplType = impl::CounterImpl<uint8_t>>
    AnyBool(std::string name, std::string labelsString, bool value, ImplType&& impl = ImplType{})
        : MetricBase(std::move(name), std::move(labelsString))
        , impl::AnyCounterBase<uint8_t>(std::forward<ImplType>(impl))
    {
        *this = value;
    }

    /**
     * @brief Set the value of the bool metric.
     * @param value The value to set.
     * @return A reference to the metric.
     */
    AnyBool&
    operator=(bool value)
    {
        pimpl_->set(value ? 1 : 0);
        return *this;
    }

    /**
     * @brief Get the value of the bool metric.
     * @return The value of the metric.
     */
    operator bool() const
    {
        auto const value = pimpl_->value();
        ASSERT(value == 0 || value == 1, "Invalid value for bool: {}", value);
        return value == 1;
    }

    /**
     * @brief Get the value of the bool metric.
     * @return The value of the metric.
     */
    std::uint8_t
    value() const
    {
        return pimpl_->value();
    }

    /**
     * @brief Serialize the metric to the given stream.
     * @param stream The stream to serialize to.
     */
    void
    serializeValue(OStream& stream) const override
    {
        stream << name() << labelsString() << ' ' << value();
    }
};

}  // namespace util::prometheus
