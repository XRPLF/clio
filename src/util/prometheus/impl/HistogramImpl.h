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

#include <util/Concepts.h>
#include <util/prometheus/OStream.h>

namespace util::prometheus::detail {

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

    HistogramImpl() = default;
    HistogramImpl(HistogramImpl const&) = delete;
    HistogramImpl(HistogramImpl&&) = default;
    HistogramImpl&
    operator=(HistogramImpl const&) = delete;
    HistogramImpl&
    operator=(HistogramImpl&&) = default;

    void
    setBuckets(std::vector<ValueType> const& bounds)
    {
        std::scoped_lock lock{*mutex_};
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
        std::scoped_lock lock{*mutex_};
        if (bucket != buckets_.end()) {
            ++bucket->count;
        } else {
            ++lastBucket_.count;
        }
        sum_ += value;
    }

    void
    serializeValue(std::string const& name, OStream& stream) const
    {
        std::scoped_lock lock{*mutex_};
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
        std::uint64_t count = 0;
    };

    std::vector<Bucket> buckets_;
    Bucket lastBucket_{std::numeric_limits<ValueType>::max()};
    ValueType sum_ = 0;
    mutable std::unique_ptr<std::mutex> mutex_ = std::make_unique<std::mutex>();
};

}  // namespace util::prometheus::detail
