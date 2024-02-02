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

#include "util/prometheus/Label.h"
#include "util/prometheus/MetricBase.h"
#include "util/prometheus/MetricBuilder.h"
#include "util/prometheus/OStream.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace util::prometheus {

/**
 * @brief Class representing a collection of Prometheus metric with the same name and type
 */
class MetricsFamily {
public:
    static std::unique_ptr<MetricBuilderInterface> defaultMetricBuilder;

    MetricsFamily(
        std::string name,
        std::optional<std::string> description,
        MetricType type,
        MetricBuilderInterface& builder = *defaultMetricBuilder
    );

    MetricsFamily(MetricsFamily const&) = delete;
    MetricsFamily(MetricsFamily&&) = default;
    MetricsFamily&
    operator=(MetricsFamily const&) = delete;
    MetricsFamily&
    operator=(MetricsFamily&&) = delete;

    /**
     * @brief Get the metric with the given labels. If it does not exist, it will be created
     *
     * @param labels The labels of the metric
     * @param buckets The buckets of the histogram. It is ignored for other metric types or if the metric already exists
     * @return Reference to the metric
     */
    MetricBase&
    getMetric(Labels labels, std::vector<std::int64_t> const& buckets = {});

    /**
     * @brief Get the metric with the given labels. If it does not exist, it will be created
     *
     * @note This overload is only used for histograms with integer buckets
     *
     * @param labels The labels of the metric
     * @param buckets The buckets of the histogram. It is ignored for other metric types or if the metric already exists
     * @return Reference to the metric
     */
    MetricBase&
    getMetric(Labels labels, std::vector<double> const& buckets);

    /**
     * @brief Serialize the metrics to a string in Prometheus format as one block
     *
     * @param stream The stream to serialize into
     * @param metricsFamily The metrics to serialize
     */
    friend OStream&
    operator<<(OStream& stream, MetricsFamily const& metricsFamily);

    std::string const&
    name() const;

    MetricType
    type() const;

private:
    std::string name_;
    std::optional<std::string> description_;
    std::unordered_map<std::string, std::unique_ptr<MetricBase>> metrics_;
    MetricType type_;
    MetricBuilderInterface& metricBuilder_;

    template <typename ValueType>
        requires std::same_as<ValueType, std::int64_t> || std::same_as<ValueType, double>
    MetricBase&
    getMetricImpl(Labels labels, std::vector<ValueType> const& buckets);
};

}  // namespace util::prometheus
