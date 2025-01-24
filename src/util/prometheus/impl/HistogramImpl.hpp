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
#include "util/Concepts.hpp"
#include "util/Mutex.hpp"
#include "util/prometheus/OStream.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace util::prometheus::impl {

template <typename T>
concept SomeHistogramImpl = requires(T t) {
    typename std::remove_cvref_t<T>::ValueType;
    requires SomeNumberType<typename std::remove_cvref_t<T>::ValueType>;
    { t.observe(typename std::remove_cvref_t<T>::ValueType{1}) } -> std::same_as<void>;
    { t.setBuckets(std::vector<typename std::remove_cvref_t<T>::ValueType>{}) } -> std::same_as<void>;
    { t.serializeValue(std::string{}, std::string{}, std::declval<OStream&>()) } -> std::same_as<void>;
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
        auto data = data_->template lock<std::scoped_lock>();
        ASSERT(data->buckets.empty(), "Buckets can be set only once.");
        data->buckets.reserve(bounds.size());
        for (auto const& bound : bounds) {
            data->buckets.emplace_back(bound);
        }
    }

    void
    observe(ValueType const value)
    {
        auto data = data_->template lock<std::scoped_lock>();
        auto const bucket = std::lower_bound(
            data->buckets.begin(),
            data->buckets.end(),
            value,
            [](Bucket const& bucket, ValueType const& value) { return bucket.upperBound < value; }
        );
        if (bucket != data->buckets.end()) {
            ++bucket->count;
        } else {
            ++data->lastBucket.count;
        }
        data->sum += value;
    }

    void
    serializeValue(std::string const& name, std::string labelsString, OStream& stream) const
    {
        if (labelsString.empty()) {
            labelsString = "{";
        } else {
            ASSERT(
                labelsString.front() == '{' && labelsString.back() == '}',
                "Labels must be in Prometheus serialized format."
            );
            labelsString.back() = ',';
        }

        auto data = data_->template lock<std::scoped_lock>();
        std::uint64_t cumulativeCount = 0;

        for (auto const& bucket : data->buckets) {
            cumulativeCount += bucket.count;
            stream << name << "_bucket" << labelsString << "le=\"" << bucket.upperBound << "\"} " << cumulativeCount
                   << '\n';
        }
        cumulativeCount += data->lastBucket.count;
        stream << name << "_bucket" << labelsString << "le=\"+Inf\"} " << cumulativeCount << '\n';

        if (labelsString.size() == 1) {
            labelsString = "";
        } else {
            labelsString.back() = '}';
        }
        stream << name << "_sum" << labelsString << " " << data->sum << '\n';
        stream << name << "_count" << labelsString << " " << cumulativeCount << '\n';
    }

private:
    struct Bucket {
        Bucket(ValueType upperBound) : upperBound(upperBound)
        {
        }

        ValueType upperBound;
        std::uint64_t count = 0;
    };

    struct Data {
        std::vector<Bucket> buckets;
        Bucket lastBucket{std::numeric_limits<ValueType>::max()};
        ValueType sum = 0;
    };
    std::unique_ptr<util::Mutex<Data>> data_ = std::make_unique<util::Mutex<Data>>();
};

}  // namespace util::prometheus::impl
