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

#include "util/prometheus/MetricBase.hpp"
#include "util/prometheus/OStream.hpp"
#include "util/prometheus/impl/AnyCounterBase.hpp"

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace util::prometheus {

/**
 * @brief A prometheus gauge metric implementation. It can be increased, decreased or set to a value.
 */
template <SomeNumberType NumberType>
struct AnyGauge : MetricBase, detail::AnyCounterBase<NumberType> {
    using ValueType = NumberType;

    /**
     * @brief Construct a new AnyGauge object
     *
     * @param name The name of the gauge
     * @param labelsString The labels of the gauge
     * @param impl The implementation of the counter inside the gauge
     */
    template <detail::SomeCounterImpl ImplType = detail::CounterImpl<ValueType>>
        requires std::same_as<ValueType, typename std::remove_cvref_t<ImplType>::ValueType>
    AnyGauge(std::string name, std::string labelsString, ImplType&& impl = ImplType{})
        : MetricBase(std::move(name), std::move(labelsString))
        , detail::AnyCounterBase<ValueType>(std::forward<ImplType>(impl))
    {
    }

    /**
     * @brief Increase the gauge by one
     */
    AnyGauge&
    operator++()
    {
        this->pimpl_->add(ValueType{1});
        return *this;
    }

    /**
     * @brief Decrease the gauge by one
     */
    AnyGauge&
    operator--()
    {
        this->pimpl_->add(ValueType{-1});
        return *this;
    }

    /**
     * @brief Increase the gauge by the given value
     *
     * @param value The value to increase the gauge by
     */
    AnyGauge&
    operator+=(ValueType const value)
    {
        this->pimpl_->add(value);
        return *this;
    }

    /**
     * @brief Decrease the gauge by the given value
     *
     * @param value The value to decrease the gauge by
     */
    AnyGauge&
    operator-=(ValueType const value)
    {
        this->pimpl_->add(-value);
        return *this;
    }

    /**
     * @brief Set the gauge to the given value
     *
     * @param value The value to set the gauge to
     */
    void
    set(ValueType const value)
    {
        this->pimpl_->set(value);
    }

    /**
     * @brief Get the value of the counter
     */
    ValueType
    value() const
    {
        return this->pimpl_->value();
    }

    /**
     * @brief Serialize the counter to a string in prometheus format (i.e. name{labels} value)
     *
     * @param stream The stream to serialize into
     */
    void
    serializeValue(OStream& stream) const override
    {
        stream << name() << labelsString() << ' ' << value();
    }
};

using GaugeInt = AnyGauge<std::int64_t>;
using GaugeDouble = AnyGauge<double>;

}  // namespace util::prometheus
