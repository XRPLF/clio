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

#include "util/prometheus/OStream.hpp"

#include <string>

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
     * @param stream The stream to serialize into
     * @param metricBase The metric to serialize
     */
    friend OStream&
    operator<<(OStream& stream, MetricBase const& metricBase);

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
     * @return The serialized value
     */
    virtual void
    serializeValue(OStream& stream) const = 0;

private:
    std::string name_;
    std::string labelsString_;
};

enum class MetricType {
    COUNTER_INT,
    COUNTER_DOUBLE,
    GAUGE_INT,
    GAUGE_DOUBLE,
    HISTOGRAM_INT,
    HISTOGRAM_DOUBLE,
    SUMMARY
};

char const*
toString(MetricType type);

}  // namespace util::prometheus
