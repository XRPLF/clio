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

#include <util/prometheus/Label.h>

#include <fmt/format.h>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

namespace util::prometheus {

/**
 * @brief Base class for a Prometheus metric containing a name and labels
 */
class MetricBase {
public:
    MetricBase(std::string name, std::string labelsString);

    MetricBase(MetricBase const&) = delete;
    MetricBase(MetricBase&&) = default;
    MetricBase&
    operator=(MetricBase const&) = delete;
    MetricBase&
    operator=(MetricBase&&) = default;
    virtual ~MetricBase() = default;

    /**
     * @brief Serialize the metric to a string in Prometheus format
     *
     * @param s The string to serialize into
     */
    void
    serialize(std::string& s) const;

    /**
     * @brief Get the name of the metric
     */
    std::string const&
    name() const;

    /**
     * @brief Get the labels of the metric in serialized format, e.g. {name="value",name2="value2"}
     */
    std::string const&
    labelsString() const;

protected:
    /**
     * @brief Interface to serialize the value of the metric
     *
     * @param result The string to serialize into
     */
    virtual void
    serializeValue(std::string& result) const = 0;

private:
    std::string name_;
    std::string labelsString_;
};

enum class MetricType { COUNTER_INT, COUNTER_DOUBLE, GAUGE_INT, GAUGE_DOUBLE, HISTOGRAM, SUMMARY };

char const*
toString(MetricType type);

/**
 * @brief Class representing a collection of Prometheus metric with the same name and type
 */
class MetricsFamily {
public:
    using MetricBuilder = std::function<std::unique_ptr<MetricBase>(std::string, std::string, MetricType)>;
    static MetricBuilder defaultMetricBuilder;

    MetricsFamily(
        std::string name,
        std::optional<std::string> description,
        MetricType type,
        MetricBuilder& builder = defaultMetricBuilder
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
     * @return Reference to the metric
     */
    MetricBase&
    getMetric(Labels labels);

    /**
     * @brief Serialize all the containing metrics to a string in Prometheus format as one block
     *
     * @param result The string to serialize into
     */
    void
    serialize(std::string& result) const;

    std::string const&
    name() const;

    MetricType
    type() const;

private:
    std::string name_;
    std::optional<std::string> description_;
    std::unordered_map<std::string, std::unique_ptr<MetricBase>> metrics_;
    MetricType type_;
    MetricBuilder& metricBuilder_;
};

}  // namespace util::prometheus
