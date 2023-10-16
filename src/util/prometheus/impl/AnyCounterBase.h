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
#include <util/prometheus/impl/CounterImpl.h>

namespace util::prometheus::impl {

template <SomeNumberType NumberType>
class AnyCounterBase : public MetricBase
{
public:
    using ValueType = NumberType;

    template <SomeCounterImpl ImplType = CounterImpl<ValueType>>
    requires std::same_as<ValueType, typename ImplType::ValueType>
    AnyCounterBase(std::string name, std::string labelsString, ImplType&& impl = ImplType{})
        : MetricBase(std::move(name), std::move(labelsString))
        , pimpl_(std::make_unique<Model<ImplType>>(std::forward<ImplType>(impl)))
    {
    }

protected:
    struct Concept
    {
        virtual ~Concept() = default;

        virtual void change(ValueType) = 0;

        virtual void
        reset() = 0;

        virtual ValueType
        value() const = 0;
    };

    template <SomeCounterImpl ImplType>
    struct Model : Concept
    {
        Model(ImplType&& impl) : impl_(std::move(impl))
        {
        }

        void
        change(ValueType value) override
        {
            impl_.change(value);
        }

        void
        reset() override
        {
            impl_.reset();
        }

        ValueType
        value() const override
        {
            return impl_.value();
        }

        ImplType impl_;
    };

    std::unique_ptr<Concept> pimpl_;
};

}  // namespace util::prometheus::impl
