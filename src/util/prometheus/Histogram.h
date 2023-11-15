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

#include <util/prometheus/MetricBase.h>
#include <util/prometheus/impl/HistogramImpl.h>

namespace util::prometheus {

template <SomeNumberType NumberType>
class AnyHistogram : public MetricBase {
public:
    using ValueType = NumberType;
    using Buckets = std::vector<NumberType>;

    template <detail::SomeHistogramImpl ImplType = detail::HistogramImpl<ValueType>>
        requires std::same_as<ValueType, typename std::remove_cvref_t<ImplType>::ValueType>
    AnyHistogram(std::string name, std::string labelsString, Buckets const& buckets, ImplType&& impl = ImplType{})
        : MetricBase(std::move(name), std::move(labelsString))
        , pimpl_(std::make_unique<Model<ImplType>>(std::forward<ImplType>(impl)))
    {
        assert(!buckets.empty());
        assert(std::is_sorted(buckets.begin(), buckets.end()));
        pimpl_->setBuckets(buckets);
    }

    void
    observe(ValueType const value)
    {
        pimpl_->observe(value);
    }

    void
    serializeValue(OStream& stream) const override
    {
        pimpl_->serializeValue(name(), labelsString(), stream);
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual void observe(NumberType) = 0;

        virtual void
        setBuckets(Buckets const& buckets) = 0;

        virtual void
        serializeValue(std::string const& name, std::string const& labelsString, OStream&) const = 0;
    };

    template <detail::SomeHistogramImpl ImplType>
        requires std::same_as<NumberType, typename std::remove_cvref_t<ImplType>::ValueType>
    struct Model : Concept {
        template <typename SomeImplType>
            requires std::same_as<SomeImplType, ImplType>
        Model(SomeImplType&& impl) : impl_(std::forward<SomeImplType>(impl))
        {
        }

        void
        observe(NumberType value) override
        {
            impl_.observe(value);
        }

        void
        setBuckets(Buckets const& buckets) override
        {
            impl_.setBuckets(buckets);
        }

        void
        serializeValue(std::string const& name, std::string const& labelsString, OStream& stream) const override
        {
            impl_.serializeValue(name, labelsString, stream);
        }

    private:
        ImplType impl_;
    };

    std::unique_ptr<Concept> pimpl_;
};

using HistogramInt = AnyHistogram<std::int64_t>;
using HistogramDouble = AnyHistogram<double>;

}  // namespace util::prometheus
