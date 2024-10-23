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

#include "util/prometheus/Prometheus.hpp"

#include "util/Assert.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Histogram.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/MetricBase.hpp"
#include "util/prometheus/MetricsFamily.hpp"
#include "util/prometheus/OStream.hpp"

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace util::prometheus {

namespace {

template <typename MetricType>
MetricType&
convertBaseTo(MetricBase& metricBase)
{
    auto result = dynamic_cast<MetricType*>(&metricBase);
    ASSERT(result != nullptr, "Failed to cast metric {} to the requested type", metricBase.name());
    return *result;
}

}  // namespace

Bool
PrometheusImpl::boolMetric(std::string name, Labels labels, std::optional<std::string> description)
{
    auto& metric = gaugeInt(std::move(name), std::move(labels), std::move(description));
    return Bool{metric};
}

CounterInt&
PrometheusImpl::counterInt(std::string name, Labels labels, std::optional<std::string> description)
{
    MetricBase& metricBase =
        getMetric(std::move(name), std::move(labels), std::move(description), MetricType::COUNTER_INT);
    return convertBaseTo<CounterInt>(metricBase);
}

CounterDouble&
PrometheusImpl::counterDouble(std::string name, Labels labels, std::optional<std::string> description)
{
    MetricBase& metricBase =
        getMetric(std::move(name), std::move(labels), std::move(description), MetricType::COUNTER_DOUBLE);
    return convertBaseTo<CounterDouble>(metricBase);
}

GaugeInt&
PrometheusImpl::gaugeInt(std::string name, Labels labels, std::optional<std::string> description)
{
    MetricBase& metricBase =
        getMetric(std::move(name), std::move(labels), std::move(description), MetricType::GAUGE_INT);
    return convertBaseTo<GaugeInt>(metricBase);
}

GaugeDouble&
PrometheusImpl::gaugeDouble(std::string name, Labels labels, std::optional<std::string> description)
{
    MetricBase& metricBase =
        getMetric(std::move(name), std::move(labels), std::move(description), MetricType::GAUGE_DOUBLE);
    return convertBaseTo<GaugeDouble>(metricBase);
}

HistogramInt&
PrometheusImpl::histogramInt(
    std::string name,
    Labels labels,
    std::vector<std::int64_t> const& buckets,
    std::optional<std::string> description
)
{
    MetricBase& metricBase =
        getMetric(std::move(name), std::move(labels), std::move(description), MetricType::HISTOGRAM_INT, buckets);
    return convertBaseTo<HistogramInt>(metricBase);
}

HistogramDouble&
PrometheusImpl::histogramDouble(
    std::string name,
    Labels labels,
    std::vector<double> const& buckets,
    std::optional<std::string> description
)
{
    MetricBase& metricBase =
        getMetric(std::move(name), std::move(labels), std::move(description), MetricType::HISTOGRAM_DOUBLE, buckets);
    return convertBaseTo<HistogramDouble>(metricBase);
}

std::string
PrometheusImpl::collectMetrics()
{
    if (!isEnabled())
        return {};

    OStream stream{compressReplyEnabled()};

    for (auto const& [name, family] : metrics_) {
        stream << family;
    }
    return std::move(stream).data();
}

MetricsFamily&
PrometheusImpl::getMetricsFamily(std::string name, std::optional<std::string> description, MetricType type)
{
    auto it = metrics_.find(name);
    if (it == metrics_.end()) {
        auto nameCopy = name;
        it = metrics_.emplace(std::move(nameCopy), MetricsFamily(std::move(name), std::move(description), type)).first;
    } else if (it->second.type() != type) {
        throw std::runtime_error("Metrics of different type can't have the same name: " + name);
    }
    return it->second;
}

MetricBase&
PrometheusImpl::getMetric(
    std::string name,
    Labels labels,
    std::optional<std::string> description,
    MetricType const type
)
{
    auto& metricFamily = getMetricsFamily(std::move(name), std::move(description), type);
    return metricFamily.getMetric(std::move(labels));
}

template <typename ValueType>
    requires std::same_as<ValueType, std::int64_t> || std::same_as<ValueType, double>
MetricBase&
PrometheusImpl::getMetric(
    std::string name,
    Labels labels,
    std::optional<std::string> description,
    MetricType type,
    std::vector<ValueType> const& buckets
)
{
    auto& metricFamily = getMetricsFamily(std::move(name), std::move(description), type);
    return metricFamily.getMetric(std::move(labels), buckets);
}

}  // namespace util::prometheus

void
PrometheusService::init(util::config::ClioConfigDefinition const& config)
{
    bool const enabled = config.getValue<bool>("prometheus.enabled");
    bool const compressReply = config.getValue<bool>("prometheus.compress_reply");

    instance_ = std::make_unique<util::prometheus::PrometheusImpl>(enabled, compressReply);
}

util::prometheus::Bool
PrometheusService::boolMetric(std::string name, util::prometheus::Labels labels, std::optional<std::string> description)
{
    return instance().boolMetric(std::move(name), std::move(labels), std::move(description));
}

util::prometheus::CounterInt&
PrometheusService::counterInt(std::string name, util::prometheus::Labels labels, std::optional<std::string> description)
{
    return instance().counterInt(std::move(name), std::move(labels), std::move(description));
}

util::prometheus::CounterDouble&
PrometheusService::counterDouble(
    std::string name,
    util::prometheus::Labels labels,
    std::optional<std::string> description
)
{
    return instance().counterDouble(std::move(name), std::move(labels), std::move(description));
}

util::prometheus::GaugeInt&
PrometheusService::gaugeInt(std::string name, util::prometheus::Labels labels, std::optional<std::string> description)
{
    return instance().gaugeInt(std::move(name), std::move(labels), std::move(description));
}

util::prometheus::GaugeDouble&
PrometheusService::gaugeDouble(
    std::string name,
    util::prometheus::Labels labels,
    std::optional<std::string> description
)
{
    return instance().gaugeDouble(std::move(name), std::move(labels), std::move(description));
}

util::prometheus::HistogramInt&
PrometheusService::histogramInt(
    std::string name,
    util::prometheus::Labels labels,
    std::vector<std::int64_t> const& buckets,
    std::optional<std::string> description
)
{
    return instance().histogramInt(std::move(name), std::move(labels), buckets, std::move(description));
}

util::prometheus::HistogramDouble&
PrometheusService::histogramDouble(
    std::string name,
    util::prometheus::Labels labels,
    std::vector<double> const& buckets,
    std::optional<std::string> description
)
{
    return instance().histogramDouble(std::move(name), std::move(labels), buckets, std::move(description));
}

std::string
PrometheusService::collectMetrics()
{
    return instance().collectMetrics();
}

bool
PrometheusService::isEnabled()
{
    return instance().isEnabled();
}

bool
PrometheusService::compressReplyEnabled()
{
    return instance().compressReplyEnabled();
}

void
PrometheusService::replaceInstance(std::unique_ptr<util::prometheus::PrometheusInterface> instance)
{
    instance_ = std::move(instance);
}

util::prometheus::PrometheusInterface&
PrometheusService::instance()
{
    ASSERT(instance_ != nullptr, "PrometheusService::instance() called before init()");
    return *instance_;
}

std::unique_ptr<util::prometheus::PrometheusInterface> PrometheusService::instance_;
