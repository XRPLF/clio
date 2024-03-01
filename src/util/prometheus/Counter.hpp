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
#include <string>
#include <type_traits>
#include <utility>

namespace util::prometheus {

/**
 * @brief A prometheus counter metric implementation. It can only be increased or be reset to zero.
 *
 * @tparam NumberType The type of the value of the counter
 */
template <SomeNumberType NumberType>
struct AnyCounter : MetricBase, impl::AnyCounterBase<NumberType> {
    using ValueType = NumberType;

    /**
     * @brief Construct a new AnyCounter object
     *
     * @tparam ImplType The type of the implementation of the counter
     * @param name The name of the counter
     * @param labelsString The labels of the counter
     * @param impl The implementation of the counter
     */
    template <impl::SomeCounterImpl ImplType = impl::CounterImpl<ValueType>>
        requires std::same_as<ValueType, typename std::remove_cvref_t<ImplType>::ValueType>
    AnyCounter(std::string name, std::string labelsString, ImplType&& impl = ImplType{})
        : MetricBase(std::move(name), std::move(labelsString))
        , impl::AnyCounterBase<ValueType>(std::forward<ImplType>(impl))
    {
    }

    /**
     * @brief Increase the counter by one
     *
     * @return Reference to self
     */
    AnyCounter&
    operator++()
    {
        this->pimpl_->add(ValueType{1});
        return *this;
    }

    /**
     * @brief Increase the counter by the given value
     *
     * @param value The value to increase the counter by
     *
     * @return Reference to self
     */
    AnyCounter&
    operator+=(ValueType const value)
    {
        ASSERT(value >= 0, "Cannot decrease a counter");
        this->pimpl_->add(value);
        return *this;
    }

    /**
     * @brief Reset the counter to zero
     */
    void
    reset()
    {
        this->pimpl_->set(ValueType{0});
    }

    /**
     * @brief Get the value of the counter
     *
     * @return The value
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

/**
 * @brief A prometheus counter metric implementation with a value of type std::int64_t
 */
using CounterInt = AnyCounter<std::uint64_t>;

/**
 * @brief A prometheus counter metric implementation with a value of type double
 */
using CounterDouble = AnyCounter<double>;

}  // namespace util::prometheus
