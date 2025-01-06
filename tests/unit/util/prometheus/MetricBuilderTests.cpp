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

#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Histogram.hpp"
#include "util/prometheus/MetricBase.hpp"
#include "util/prometheus/MetricBuilder.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace util::prometheus;

TEST(MetricBuilderDeathTest, build)
{
    std::string const name = "name";
    std::string const labelsString = "{label1=\"value1\"}";
    MetricBuilder builder;
    for (auto const type :
         {MetricType::CounterInt,
          MetricType::CounterDouble,
          MetricType::GaugeInt,
          MetricType::GaugeDouble,
          MetricType::HistogramInt,
          MetricType::HistogramDouble}) {
        std::unique_ptr<MetricBase> metric = [&]() {
            if (type == MetricType::HistogramInt)
                return builder(name, labelsString, type, std::vector<std::int64_t>{1});

            if (type == MetricType::HistogramDouble)
                return builder(name, labelsString, type, std::vector<double>{1.});

            return builder(name, labelsString, type);
        }();
        switch (type) {
            case MetricType::CounterInt:
                EXPECT_NE(dynamic_cast<CounterInt*>(metric.get()), nullptr);
                break;
            case MetricType::CounterDouble:
                EXPECT_NE(dynamic_cast<CounterDouble*>(metric.get()), nullptr);
                break;
            case MetricType::GaugeInt:
                EXPECT_NE(dynamic_cast<GaugeInt*>(metric.get()), nullptr);
                break;
            case MetricType::GaugeDouble:
                EXPECT_NE(dynamic_cast<GaugeDouble*>(metric.get()), nullptr);
                break;
            case MetricType::HistogramInt:
                EXPECT_NE(dynamic_cast<HistogramInt*>(metric.get()), nullptr);
                break;
            case MetricType::HistogramDouble:
                EXPECT_NE(dynamic_cast<HistogramDouble*>(metric.get()), nullptr);
                break;
            default:
                EXPECT_EQ(metric, nullptr);
        }
        if (metric != nullptr) {
            EXPECT_EQ(metric->name(), name);
            EXPECT_EQ(metric->labelsString(), labelsString);
        }
    }
    EXPECT_DEATH({ builder(name, labelsString, MetricType::Summary, std::vector<std::int64_t>{}); }, "");
}
