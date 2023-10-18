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
    for (auto& [name, family] : metrics_)
    {
        family.serialize(result);
    }
    return result;
}

bool
PrometheusImpl::isEnabled() const
{
    return true;
}

MetricBase&
PrometheusImpl::getMetric(
    std::string name,
    Labels labels,
    std::optional<std::string> description,
    MetricType const type)
{
    auto it = metrics_.find(name);
    if (it == metrics_.end())
    {
        auto nameCopy = name;
        it = metrics_.emplace(std::move(nameCopy), MetricsFamily(std::move(name), std::move(description), type)).first;
    }
    return it->second.getMetric(std::move(labels));
}

CounterInt& PromeseusDisabled::counterInt(std::string, Labels, std::optional<std::string>)
{
    static CounterInt dummy("", "", impl::CounterDisabledImpl<std::uint64_t>{});
    return dummy;
}

CounterDouble& PromeseusDisabled::counterDouble(std::string, Labels, std::optional<std::string>)
{
    static CounterDouble dummy("", "", impl::CounterDisabledImpl<double>{});
    return dummy;
}

GaugeInt& PromeseusDisabled::gaugeInt(std::string, Labels, std::optional<std::string>)
{
    static GaugeInt dummy("", "", impl::CounterDisabledImpl<std::int64_t>{});
    return dummy;
}

GaugeDouble& PromeseusDisabled::gaugeDouble(std::string, Labels, std::optional<std::string>)
{
    static GaugeDouble dummy("", "", impl::CounterDisabledImpl<double>{});
    return dummy;
}

std::string
PromeseusDisabled::collectMetrics()
{
    return "";
}

bool
PromeseusDisabled::isEnabled() const
{
    return false;
}

std::unique_ptr<PrometheusInterface> PrometheusSingleton::instance_{};

}  // namespace util::prometheus
