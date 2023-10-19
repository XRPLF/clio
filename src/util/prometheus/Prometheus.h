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

#include <util/config/Config.h>
#include <util/prometheus/Counter.h>
#include <util/prometheus/Gauge.h>

#include <cassert>

namespace util::prometheus {

class PrometheusInterface
{
public:
    PrometheusInterface(bool isEnabled) : isEnabled_(isEnabled)
    {
    }

    virtual ~PrometheusInterface() = default;

    /**
     * @brief Get a integer based counter metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     */
    virtual CounterInt&
    counterInt(std::string name, Labels labels, std::optional<std::string> description = std::nullopt) = 0;

    /**
     * @brief Get a double based counter metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     */
    virtual CounterDouble&
    counterDouble(std::string name, Labels labels, std::optional<std::string> description = std::nullopt) = 0;

    /**
     * @brief Get a integer based gauge metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     */
    virtual GaugeInt&
    gaugeInt(std::string name, Labels labels, std::optional<std::string> description = std::nullopt) = 0;

    /**
     * @brief Get a double based gauge metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     */
    virtual GaugeDouble&
    gaugeDouble(std::string name, Labels labels, std::optional<std::string> description = std::nullopt) = 0;

    /**
     * @brief Collect all metrics and return them as a string in Prometheus format
     *
     * @return The serialized metrics
     */
    virtual std::string
    collectMetrics() = 0;

    /**
     * @brief Whether prometheus is enabled
     *
     * @return true if prometheus is enabled
     */
    bool
    isEnabled() const
    {
        return isEnabled_;
    }

    /**
     * @brief Whether prometheus has registered metrics
     *
     * @return true if prometheus has already registered some metrics
     */
    virtual bool
    hasMetrics() const = 0;

private:
    bool isEnabled_;
};

/**
 * @brief Implemetation of PrometheusInterface
 *
 * @note When prometheus is disabled, all metrics will still counted but collection is disabled
 */
class PrometheusImpl : public PrometheusInterface
{
public:
    using PrometheusInterface::PrometheusInterface;

    CounterInt&
    counterInt(std::string name, Labels labels, std::optional<std::string> description) override;

    CounterDouble&
    counterDouble(std::string name, Labels labels, std::optional<std::string> description) override;

    GaugeInt&
    gaugeInt(std::string name, Labels labels, std::optional<std::string> description) override;

    GaugeDouble&
    gaugeDouble(std::string name, Labels labels, std::optional<std::string> description) override;

    std::string
    collectMetrics() override;

    bool
    hasMetrics() const override;

private:
    MetricBase&
    getMetric(std::string name, Labels labels, std::optional<std::string> description, MetricType type);

    std::unordered_map<std::string, MetricsFamily> metrics_;
    friend class PrometheusSingleton;
};

class PrometheusSingleton
{
public:
    void static init(Config const& config = Config{})
    {
        bool const enabled = config.valueOr("prometheus_enabled", true);
        instance_ = std::make_unique<PrometheusImpl>(enabled);
    }

    static PrometheusInterface&
    instance()
    {
        assert(instance_);
        return *instance_;
    }

    static void
    replaceInstance(std::unique_ptr<PrometheusInterface> instance)
    {
        if (instance_)
            assert(!instance_->hasMetrics());

        instance_ = std::move(instance);
    }

private:
    static std::unique_ptr<PrometheusInterface> instance_;
};

}  // namespace util::prometheus

#define PROMETHEUS util::prometheus::PrometheusSingleton::instance
#define PROMETHEUS_INIT util::prometheus::PrometheusSingleton::init
