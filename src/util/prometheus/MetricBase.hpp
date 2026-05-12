#pragma once

#include "util/prometheus/OStream.hpp"

#include <string>

namespace util::prometheus {

/**
 * @brief Base class for a Prometheus metric containing a name and labels
 */
class MetricBase {
public:
    /**
     * @brief Construct a new MetricBase object
     *
     * @param name The name of the metric
     * @param labelsString The labels of the metric in serialized format, e.g.
     * {name="value",name2="value2"}
     */
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
     * @return The name of the metric
     */
    [[nodiscard]] std::string const&
    name() const;

    /**
     * @brief Get the labels of the metric in serialized format, e.g. {name="value",name2="value2"}
     * @return The labels of the metric
     */
    [[nodiscard]] std::string const&
    labelsString() const;

protected:
    /**
     * @brief Interface to serialize the value of the metric
     *
     * @param stream The stream to serialize into
     */
    virtual void
    serializeValue(OStream& stream) const = 0;

private:
    std::string name_;
    std::string labelsString_;
};

enum class MetricType {
    CounterInt,
    CounterDouble,
    GaugeInt,
    GaugeDouble,
    HistogramInt,
    HistogramDouble,
    Summary
};

char const*
toString(MetricType type);

}  // namespace util::prometheus
