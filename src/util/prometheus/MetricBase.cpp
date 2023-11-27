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

#include "util/prometheus/MetricBase.h"

#include "util/Assert.h"
#include "util/prometheus/OStream.h"

#include <string>
#include <utility>

namespace util::prometheus {

MetricBase::MetricBase(std::string name, std::string labelsString)
    : name_(std::move(name)), labelsString_(std::move(labelsString))
{
}

OStream&
operator<<(OStream& stream, MetricBase const& metricBase)
{
    metricBase.serializeValue(stream);
    return stream;
}

char const*
toString(MetricType type)
{
    switch (type) {
        case MetricType::COUNTER_INT:
            [[fallthrough]];
        case MetricType::COUNTER_DOUBLE:
            return "counter";
        case MetricType::GAUGE_INT:
            [[fallthrough]];
        case MetricType::GAUGE_DOUBLE:
            return "gauge";
        case MetricType::HISTOGRAM_INT:
            [[fallthrough]];
        case MetricType::HISTOGRAM_DOUBLE:
            return "histogram";
        case MetricType::SUMMARY:
            return "summary";
        default:
            ASSERT(false, "Unknown metric {}.", static_cast<int>(type));
    }
    return "";
}

std::string const&
MetricBase::name() const
{
    return name_;
}

std::string const&
MetricBase::labelsString() const
{
    return labelsString_;
}

}  // namespace util::prometheus
