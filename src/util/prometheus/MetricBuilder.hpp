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

#include "util/prometheus/MetricBase.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace util::prometheus {

/**
 * @brief Interface to create a metric
 */
struct MetricBuilderInterface {
    virtual ~MetricBuilderInterface() = default;

    /**
     * @brief Create a metric
     *
     * @param name The name of the metric
     * @param labelsString The labels of the metric in serialized format, e.g. {name="value",name2="value2"}
     * @param type The type of the metric
     * @param buckets The buckets of the int based histogram. It is ignored for other metric types
     * @return The metric
     */
    virtual std::unique_ptr<MetricBase>
    operator()(
        std::string name,
        std::string labelsString,
        MetricType type,
        std::vector<std::int64_t> const& buckets = {}
    ) = 0;

    /**
     * @brief Create a metric double based  histogram
     *
     * @param name The name of the metric
     * @param labelsString The labels of the metric in serialized format, e.g. {name="value",name2="value2"}
     * @param type The type of the metric. Must be HISOTGRAM_DOUBLE
     * @param buckets The buckets of the histogram
     * @return Double based histogram
     */
    virtual std::unique_ptr<MetricBase>
    operator()(std::string name, std::string labelsString, MetricType type, std::vector<double> const& buckets) = 0;
};

/**
 * @brief Implementation for building a metric
 */
class MetricBuilder : public MetricBuilderInterface {
public:
    std::unique_ptr<MetricBase>
    operator()(
        std::string name,
        std::string labelsString,
        MetricType type,
        std::vector<std::int64_t> const& buckets = {}
    ) override;

    std::unique_ptr<MetricBase>
    operator()(std::string name, std::string labelsString, MetricType type, std::vector<double> const& buckets)
        override;

private:
    std::unique_ptr<MetricBase> static makeMetric(std::string name, std::string labelsString, MetricType type);

    template <typename ValueType>
        requires std::same_as<ValueType, std::int64_t> || std::same_as<ValueType, double>
    std::unique_ptr<MetricBase>
    makeHistogram(std::string name, std::string labelsString, MetricType type, std::vector<ValueType> const& buckets);
};

}  // namespace util::prometheus
