#include "util/prometheus/MetricBase.hpp"

#include "util/Assert.hpp"
#include "util/prometheus/OStream.hpp"

#include <string>
#include <utility>

namespace util::prometheus {

MetricBase::MetricBase(std::string name, std::string labelsString)
    : name_(std::move(name)), labelsString_(std::move(labelsString))
{
}

OStream&
operator<<(OStream& stream, MetricBase const& metricBase)
{
    metricBase.serializeValue(stream);
    return stream;
}

char const*
toString(MetricType type)
{
    switch (type) {
        case MetricType::CounterInt:
            [[fallthrough]];
        case MetricType::CounterDouble:
            return "counter";
        case MetricType::GaugeInt:
            [[fallthrough]];
        case MetricType::GaugeDouble:
            return "gauge";
        case MetricType::HistogramInt:
            [[fallthrough]];
        case MetricType::HistogramDouble:
            return "histogram";
        case MetricType::Summary:
            return "summary";
        default:
            ASSERT(false, "Unknown metric {}.", static_cast<int>(type));
    }
    return "";
}

std::string const&
MetricBase::name() const
{
    return name_;
}

std::string const&
MetricBase::labelsString() const
{
    return labelsString_;
}

}  // namespace util::prometheus
