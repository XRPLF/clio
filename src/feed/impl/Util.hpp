//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <fmt/core.h>

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

inline util::prometheus::CounterInt&
getPublishedMessagesCounterInt(std::string const& counterName)
{
    return PrometheusService::counterInt(
        "subscriptions_published_count",
        util::prometheus::Labels({util::prometheus::Label{"stream", counterName}}),
        fmt::format("Total published messages on the {} stream", counterName)
    );
}
}  // namespace feed::impl
