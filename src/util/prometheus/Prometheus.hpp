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

#include "util/config/Config.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Histogram.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/MetricBase.hpp"
#include "util/prometheus/MetricsFamily.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace util::prometheus {

class PrometheusInterface {
public:
    /**
     * @brief Construct a new Prometheus Interface object
     *
     * @param isEnabled Whether prometheus is enabled
     * @param compressReply Whether to compress the reply
     */
    PrometheusInterface(bool isEnabled, bool compressReply)
        : isEnabled_(isEnabled), compressReplyEnabled_(compressReply)
    {
    }

    virtual ~PrometheusInterface() = default;

    /**
     * @brief Get an integer based counter metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     * @return CounterDouble& The reference to the counter object
     */
    virtual CounterInt&
    counterInt(std::string name, Labels labels, std::optional<std::string> description = std::nullopt) = 0;

    /**
     * @brief Get a double based counter metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     * @return The reference to the counter object
     */
    virtual CounterDouble&
    counterDouble(std::string name, Labels labels, std::optional<std::string> description = std::nullopt) = 0;

    /**
     * @brief Get an integer based gauge metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     * @return The reference to the gauge object
     */
    virtual GaugeInt&
    gaugeInt(std::string name, Labels labels, std::optional<std::string> description = std::nullopt) = 0;

    /**
     * @brief Get a double based gauge metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     * @return The reference to the gauge object
     */
    virtual GaugeDouble&
    gaugeDouble(std::string name, Labels labels, std::optional<std::string> description = std::nullopt) = 0;

    /**
     * @brief Get an integer based histogram metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param buckets The buckets of the metric
     * @param description The description of the metric
     * @return The reference to the histogram object
     */
    virtual HistogramInt&
    histogramInt(
        std::string name,
        Labels labels,
        std::vector<std::int64_t> const& buckets,
        std::optional<std::string> description = std::nullopt
    ) = 0;

    /**
     * @brief Get a double based histogram metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param buckets The buckets of the metric
     * @param description The description of the metric
     * @return The reference to the histogram object
     */
    virtual HistogramDouble&
    histogramDouble(
        std::string name,
        Labels labels,
        std::vector<double> const& buckets,
        std::optional<std::string> description = std::nullopt
    ) = 0;

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
     * @brief Whether to compress the reply
     *
     * @return true if the reply should be compressed
     */
    bool
    compressReplyEnabled() const
    {
        return compressReplyEnabled_;
    }

private:
    bool isEnabled_;
    bool compressReplyEnabled_;
};

/**
 * @brief Implemetation of PrometheusInterface
 *
 * @note When prometheus is disabled, all metrics will still counted but collection is disabled
 */
class PrometheusImpl : public PrometheusInterface {
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

    HistogramInt&
    histogramInt(
        std::string name,
        Labels labels,
        std::vector<std::int64_t> const& buckets,
        std::optional<std::string> description = std::nullopt
    ) override;

    HistogramDouble&
    histogramDouble(
        std::string name,
        Labels labels,
        std::vector<double> const& buckets,
        std::optional<std::string> description = std::nullopt
    ) override;

    std::string
    collectMetrics() override;

private:
    MetricsFamily&
    getMetricsFamily(std::string name, std::optional<std::string> description, MetricType type);

    MetricBase&
    getMetric(std::string name, Labels labels, std::optional<std::string> description, MetricType type);

    template <typename ValueType>
        requires std::same_as<ValueType, std::int64_t> || std::same_as<ValueType, double>
    MetricBase&
    getMetric(
        std::string name,
        Labels labels,
        std::optional<std::string> description,
        MetricType type,
        std::vector<ValueType> const& buckets
    );

    std::unordered_map<std::string, MetricsFamily> metrics_;
};

}  // namespace util::prometheus

/**
 * @brief Singleton class to access the PrometheusInterface
 */
class PrometheusService {
public:
    /**
     * @brief Initialize the singleton with the given configuration
     *
     * @param config The configuration to use
     */
    void static init(util::Config const& config = util::Config{});

    /**
     * @brief Get an integer based counter metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     */
    static util::prometheus::CounterInt&
    counterInt(
        std::string name,
        util::prometheus::Labels labels,
        std::optional<std::string> description = std::nullopt
    );

    /**
     * @brief Get a double based counter metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     */
    static util::prometheus::CounterDouble&
    counterDouble(
        std::string name,
        util::prometheus::Labels labels,
        std::optional<std::string> description = std::nullopt
    );

    /**
     * @brief Get an integer based gauge metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     */
    static util::prometheus::GaugeInt&
    gaugeInt(std::string name, util::prometheus::Labels labels, std::optional<std::string> description = std::nullopt);

    /**
     * @brief Get a double based gauge metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param description The description of the metric
     */
    static util::prometheus::GaugeDouble&
    gaugeDouble(
        std::string name,
        util::prometheus::Labels labels,
        std::optional<std::string> description = std::nullopt
    );

    /**
     * @brief Get an integer based histogram metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param buckets The buckets of the metric
     * @param description The description of the metric
     * @return The reference to the histogram object
     */
    static util::prometheus::HistogramInt&
    histogramInt(
        std::string name,
        util::prometheus::Labels labels,
        std::vector<std::int64_t> const& buckets,
        std::optional<std::string> description = std::nullopt
    );

    /**
     * @brief Get a double based histogram metric. It will be created if it doesn't exist
     *
     * @param name The name of the metric
     * @param labels The labels of the metric
     * @param buckets The buckets of the metric
     * @param description The description of the metric
     * @return The reference to the histogram object
     */
    static util::prometheus::HistogramDouble&
    histogramDouble(
        std::string name,
        util::prometheus::Labels labels,
        std::vector<double> const& buckets,
        std::optional<std::string> description = std::nullopt
    );

    /**
     * @brief Collect all metrics and return them as a string in Prometheus format
     *
     * @return The serialized metrics
     */
    static std::string
    collectMetrics();

    /**
     * @brief Whether prometheus is enabled
     *
     * @return true if prometheus is enabled
     */
    static bool
    isEnabled();

    /**
     * @brief Whether to compress the reply
     *
     * @return true if the reply should be compressed
     */
    static bool
    compressReplyEnabled();

    /**
     * @brief Replace the prometheus object stored in the singleton
     *
     * @note Be careful with this method because there could be hanging references to counters
     *
     * @param instance The new prometheus object
     */
    static void
    replaceInstance(std::unique_ptr<util::prometheus::PrometheusInterface> instance);

    /**
     * @brief Get the prometheus object stored in the singleton
     *
     * @return The prometheus object reference
     */
    static util::prometheus::PrometheusInterface&
    instance();

private:
    static std::unique_ptr<util::prometheus::PrometheusInterface> instance_;
};
