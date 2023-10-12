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

#include <util/prometheus/Metrics.h>

namespace util::prometheus {

template <typename T>
concept SomeNumberType = std::is_arithmetic_v<T> && !std::is_same_v<T, bool> && !std::is_const_v<T>;

// clang-format off
template <typename T>
concept SomeCounterImpl = requires(T a)
{
    T::ValueType;
    { a.increase(typename T::ValueType{1}) } -> std::same_as<void>;
    { a.reset() } -> std::same_as<void>;
    { a.valueImpl() } -> SomeNumberType;
};
// clang-format on

template <SomeNumberType ValueType>
class AnyCounter
{
public:
    template <typename ImplType>
    requires std::is_same_v<typename ImplType::ValueType, ValueType>&& SomeCounterImpl<ImplType>
    AnyCounter(ImplType impl) : pimpl_(std::make_unique<Model<ImplType>>(std::move(impl)))
    {
    }

    AnyCounter&
    operator++()
    {
        pimpl_->increaseImpl(ValueType{1});
        return *this;
    }

    AnyCounter&
    operator+=(ValueType const value)
    {
        assert(value >= 0);
        pimpl_->increaseImpl(value);
        return *this;
    }

    void
    reset()
    {
        pimpl_->resetImpl();
    }

    ValueType
    value() const
    {
        return pimpl_->valueImpl();
    }

private:
    struct Concept
    {
        virtual ~Concept() = default;
        virtual void increaseImpl(ValueType) = 0;
        virtual void
        resetImpl() = 0;
        virtual ValueType
        valueImpl() const = 0;
    };

    template <typename ImplType>
    struct Model : Concept
    {
        Model(ImplType impl) : impl_(std::move(impl))
        {
        }
        void
        increaseImpl(ValueType value) override
        {
            impl_.increase(value);
        }
        void
        resetImpl() override
        {
            impl_.reset();
        }
        ValueType
        value() const override
        {
            return impl_.valueImpl();
        }
        ImplType impl_;
    };
    std::unique_ptr<Concept> pimpl_;
};

template <SomeNumberType NumberType>
class NumberCounterImpl
{
public:
    using ValueType = NumberType;

    void
    increase(ValueType value)
    {
        value_ += value;
    }

    void
    reset()
    {
        value_ = 0;
    }
    ValueType
    valueImpl() const
    {
        return value_;
    }

private:
    std::atomic<ValueType> value_{0};
};

}  // namespace util::prometheus
