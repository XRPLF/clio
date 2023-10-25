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
#include <util/prometheus/Prometheus.h>

namespace util::prometheus {

namespace {

template <typename MetricType>
MetricType&
convertBaseTo(MetricBase& metricBase)
{
    auto result = dynamic_cast<MetricType*>(&metricBase);
    assert(result != nullptr);
    if (result == nullptr)
        throw std::runtime_error("Failed to convert metric type");
    return *result;
}

}  // namespace

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

std::string
PrometheusImpl::collectMetrics()
{
    std::string result;

    if (!isEnabled())
        return result;

    for (auto& [name, family] : metrics_) {
        family.serialize(result);
    }
    return result;
}

MetricBase&
PrometheusImpl::getMetric(
    std::string name,
    Labels labels,
    std::optional<std::string> description,
    MetricType const type
)
{
    auto it = metrics_.find(name);
    if (it == metrics_.end()) {
        auto nameCopy = name;
        it = metrics_.emplace(std::move(nameCopy), MetricsFamily(std::move(name), std::move(description), type)).first;
    } else if (it->second.type() != type) {
        throw std::runtime_error("Metrics of different type can't have the same name: " + name);
    }
    return it->second.getMetric(std::move(labels));
}

std::unique_ptr<PrometheusInterface> PrometheusSingleton::instance_;

}  // namespace util::prometheus
