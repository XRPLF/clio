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

#include <util/Atomic.h>
#include <util/prometheus/Metrics.h>

namespace util::prometheus {

namespace detail {

template <typename T>
concept SomeHistogramImpl = requires(T t) {
    typename std::remove_cvref_t<T>::ValueType;
    SomeNumberType<typename std::remove_cvref_t<T>::ValueType>;
    {
        t.observe(typename std::remove_cvref_t<T>::ValueType{1})
    } -> std::same_as<void>;
    {
        t.setBuckets(std::vector<typename std::remove_cvref_t<T>::ValueType>{})
    } -> std::same_as<void>;
    {
        t.serializeValue(std::string{}, std::declval<OStream&>())
    } -> std::same_as<void>;
};

template <SomeNumberType NumberType>
class HistogramImpl {
public:
    using ValueType = NumberType;

    void
    setBuckets(std::vector<ValueType> const& bounds)
    {
        assert(buckets_.empty());
        buckets_.reserve(bounds.size());
        for (auto const& bound : bounds) {
            buckets_.emplace_back(bound);
        }
    }

    void
    observe(ValueType const value)
    {
        auto const bucket =
            std::lower_bound(buckets_.begin(), buckets_.end(), value, [](Bucket const& bucket, ValueType const& value) {
                return bucket.upperBound < value;
            });
        if (bucket != buckets_.end()) {
            bucket->count.add(1);
        } else {
            lastBucket_.count.add(1);
        }
        sum_ += value;
    }

    void
    serializeValue(std::string const& name, OStream& stream) const
    {
        std::uint64_t cumulativeCount = 0;
        for (auto const& bucket : buckets_) {
            cumulativeCount += bucket.count;
            stream << name << "_bucket{le=\"" << bucket.upperBound << "\"} " << cumulativeCount << '\n';
        }
        cumulativeCount += lastBucket_.count;
        stream << name << "_bucket{le=\"+Inf\"} " << cumulativeCount << '\n';
        stream << name << "_sum " << sum_ << '\n';
        stream << name << "_count " << cumulativeCount << '\n';
    }

private:
    struct Bucket {
        Bucket(ValueType upperBound) : upperBound(upperBound)
        {
        }

        ValueType upperBound;
        Atomic<std::uint64_t> count = 0;
    };
    std::vector<Bucket> buckets_;
    Bucket lastBucket_{std::numeric_limits<ValueType>::max()};
    Atomic<ValueType> sum_ = 0;
};

}  // namespace detail

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
        pimpl_->serializeValue(name(), stream);
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual void observe(NumberType) = 0;

        virtual void
        setBuckets(Buckets const&) = 0;

        virtual void
        serializeValue(std::string const&, OStream&) const = 0;
    };

    template <detail::SomeHistogramImpl ImplType>
        requires std::same_as<NumberType, typename std::remove_cvref_t<ImplType>::ValueType>
    struct Model : Concept {
        Model(ImplType impl) : impl_(std::forward<ImplType>(impl))
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
        serializeValue(std::string const& name, OStream& stream) const override
        {
            impl_.serializeValue(name, stream);
        }

    private:
        ImplType impl_;
    };

    std::unique_ptr<Concept> pimpl_;
};

using HistogramInt = AnyHistogram<std::int64_t>;
using HistogramDouble = AnyHistogram<double>;

}  // namespace util::prometheus
