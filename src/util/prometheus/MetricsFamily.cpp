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

#include "util/prometheus/MetricsFamily.hpp"

#include "util/Assert.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/MetricBase.hpp"
#include "util/prometheus/MetricBuilder.hpp"
#include "util/prometheus/OStream.hpp"

#include <optional>
#include <string>
#include <utility>

namespace util::prometheus {

std::unique_ptr<MetricBuilderInterface> MetricsFamily::defaultMetricBuilder = std::make_unique<MetricBuilder>();

MetricsFamily::MetricsFamily(
    std::string name,
    std::optional<std::string> description,
    MetricType type,
    MetricBuilderInterface& metricBuilder
)
    : name_(std::move(name)), description_(std::move(description)), type_(type), metricBuilder_(metricBuilder)
{
}

static MetricBase&
MetricsFamily::getMetric(Labels labels, std::vector<std::int64_t> const& buckets)
{
    return getMetricImpl(std::move(labels), buckets);
}

static MetricBase&
MetricsFamily::getMetric(Labels labels, std::vector<double> const& buckets)
{
    ASSERT(type_ == MetricType::HISTOGRAM_DOUBLE, "This method is for HISTOGRAM_DOUBLE only.");
    return getMetricImpl(std::move(labels), buckets);
}

OStream&
operator<<(OStream& stream, MetricsFamily const& metricsFamily)
{
    if (metricsFamily.description_)
        stream << "# HELP " << metricsFamily.name_ << ' ' << *metricsFamily.description_ << '\n';
    stream << "# TYPE " << metricsFamily.name_ << ' ' << toString(metricsFamily.type()) << '\n';

    for (auto const& [labelsString, metric] : metricsFamily.metrics_) {
        stream << *metric << '\n';
    }
    stream << '\n';

    return stream;
}

std::string const&
MetricsFamily::name() const
{
    return name_;
}

MetricType
MetricsFamily::type() const
{
    return type_;
}

template <typename ValueType>
    requires std::same_as<ValueType, std::int64_t> || std::same_as<ValueType, double>
MetricBase&
MetricsFamily::getMetricImpl(Labels labels, std::vector<ValueType> const& buckets)
{
    auto labelsString = labels.serialize();
    auto it = metrics_.find(labelsString);
    if (it == metrics_.end()) {
        auto metric = metricBuilder_(name(), labelsString, type(), buckets);
        auto [it2, success] = metrics_.emplace(std::move(labelsString), std::move(metric));
        it = it2;
    }
    return *it->second;
}

}  // namespace util::prometheus
