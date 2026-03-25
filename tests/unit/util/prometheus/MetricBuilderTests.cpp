#include "util/MockAssert.hpp"
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

struct MetricBuilderAssertTest : common::util::WithMockAssert {};

TEST_F(MetricBuilderAssertTest, build)
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
    EXPECT_CLIO_ASSERT_FAIL({
        builder(name, labelsString, MetricType::Summary, std::vector<std::int64_t>{});
    });
}
