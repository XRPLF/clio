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
     * @return The stream, for chaining
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

/**
 * @brief The kind and value type of a Prometheus metric
 */
enum class MetricType {
    CounterInt,       ///< Counter with integer values
    CounterDouble,    ///< Counter with floating point values
    GaugeInt,         ///< Gauge with integer values
    GaugeDouble,      ///< Gauge with floating point values
    HistogramInt,     ///< Histogram with integer values
    HistogramDouble,  ///< Histogram with floating point values
    Summary,          ///< Summary (not currently supported by the metric builder)
};

/**
 * @brief Get the Prometheus type name of a metric type, as used in the `# TYPE` line
 *
 * @param type The metric type
 * @return "counter", "gauge", "histogram" or "summary"
 */
char const*
toString(MetricType type);

}  // namespace util::prometheus
