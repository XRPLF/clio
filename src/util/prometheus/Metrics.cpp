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
#include <util/prometheus/Counter.h>
#include <util/prometheus/Gauge.h>

#include <cassert>

namespace util::prometheus {

MetricBase::MetricBase(std::string name, std::string labelsString)
    : name_(std::move(name)), labelsString_(std::move(labelsString))
{
}

void
MetricBase::serialize(std::string& s) const
{
    serializeValue(s);
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
        case MetricType::HISTOGRAM:
            return "histogram";
        case MetricType::SUMMARY:
            return "summary";
        default:
            assert(false);
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

MetricsFamily::MetricBuilder MetricsFamily::defaultMetricBuilder =
    [](std::string name, std::string labelsString, MetricType type) -> std::unique_ptr<MetricBase> {
    switch (type) {
        case MetricType::COUNTER_INT:
            return std::make_unique<CounterInt>(name, labelsString);
        case MetricType::COUNTER_DOUBLE:
            return std::make_unique<CounterDouble>(name, labelsString);
        case MetricType::GAUGE_INT:
            return std::make_unique<GaugeInt>(name, labelsString);
        case MetricType::GAUGE_DOUBLE:
            return std::make_unique<GaugeDouble>(name, labelsString);
        case MetricType::SUMMARY:
            [[fallthrough]];
        case MetricType::HISTOGRAM:
            [[fallthrough]];
        default:
            assert(false);
    }
    return nullptr;
};

MetricsFamily::MetricsFamily(
    std::string name,
    std::optional<std::string> description,
    MetricType type,
    MetricBuilder& metricBuilder
)
    : name_(std::move(name)), description_(std::move(description)), type_(type), metricBuilder_(metricBuilder)
{
}

MetricBase&
MetricsFamily::getMetric(Labels labels)
{
    auto labelsString = labels.serialize();
    auto it = metrics_.find(labelsString);
    if (it == metrics_.end()) {
        auto metric = metricBuilder_(name(), labelsString, type());
        auto [it2, success] = metrics_.emplace(std::move(labelsString), std::move(metric));
        it = it2;
    }
    return *it->second;
}

void
MetricsFamily::serialize(std::string& result) const
{
    if (description_)
        fmt::format_to(std::back_inserter(result), "# HELP {} {}\n", name_, *description_);
    fmt::format_to(std::back_inserter(result), "# TYPE {} {}\n", name_, toString(type()));

    for (auto const& [labelsString, metric] : metrics_) {
        metric->serialize(result);
        result.push_back('\n');
    }
    result.push_back('\n');
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

}  // namespace util::prometheus
