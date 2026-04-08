#pragma once

#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <fmt/format.h>

#include <string>

namespace feed::impl {

inline util::prometheus::GaugeInt&
getSubscriptionsGaugeInt(std::string const& counterName)
{
    return PrometheusService::gaugeInt(
        "subscriptions_current_number",
        util::prometheus::Labels({util::prometheus::Label{"stream", counterName}}),
        fmt::format("Current subscribers number on the {} stream", counterName)
    );
}
}  // namespace feed::impl
