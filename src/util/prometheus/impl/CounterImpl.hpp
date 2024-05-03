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

#include "util/Atomic.hpp"

#include <type_traits>

namespace util::prometheus::impl {

template <typename T>
concept SomeCounterImpl = requires(T a) {
    typename std::remove_cvref_t<T>::ValueType;
    requires SomeNumberType<typename std::remove_cvref_t<T>::ValueType>;
    { a.add(typename std::remove_cvref_t<T>::ValueType{1}) } -> std::same_as<void>;
    { a.set(typename std::remove_cvref_t<T>::ValueType{1}) } -> std::same_as<void>;
    { a.value() } -> SomeNumberType;
};

template <SomeNumberType NumberType>
class CounterImpl {
public:
    using ValueType = NumberType;

    CounterImpl() = default;

    CounterImpl(CounterImpl const&) = delete;

    CounterImpl(CounterImpl&& other) = default;

    CounterImpl&
    operator=(CounterImpl const&) = delete;
    CounterImpl&
    operator=(CounterImpl&&) = default;

    void
    add(ValueType const value)
    {
        value_->add(value);
    }

    void
    set(ValueType const value)
    {
        value_->set(value);
    }

    ValueType
    value() const
    {
        return value_->value();
    }

private:
    AtomicPtr<ValueType> value_ = std::make_unique<Atomic<ValueType>>(0);
};

}  // namespace util::prometheus::impl
