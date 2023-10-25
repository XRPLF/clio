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
#include "util/prometheus/Counter.h"
#include "util/prometheus/Gauge.h"
#include <util/prometheus/Metrics.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace util::prometheus;

TEST(DefaultMetricBuilderTest, build)
{
    std::string const name = "name";
    std::string const labelsString = "{label1=\"value1\"}";
    for (auto const type :
         {MetricType::COUNTER_INT, MetricType::COUNTER_DOUBLE, MetricType::GAUGE_INT, MetricType::GAUGE_DOUBLE}) {
        auto metric = MetricsFamily::defaultMetricBuilder(name, labelsString, type);
        switch (type) {
            case MetricType::COUNTER_INT:
                EXPECT_NE(dynamic_cast<CounterInt*>(metric.get()), nullptr);
                break;
            case MetricType::COUNTER_DOUBLE:
                EXPECT_NE(dynamic_cast<CounterDouble*>(metric.get()), nullptr);
                break;
            case MetricType::GAUGE_INT:
                EXPECT_NE(dynamic_cast<GaugeInt*>(metric.get()), nullptr);
                break;
            case MetricType::GAUGE_DOUBLE:
                EXPECT_NE(dynamic_cast<GaugeDouble*>(metric.get()), nullptr);
                break;
            default:
                EXPECT_EQ(metric, nullptr);
        }
        if (metric != nullptr) {
            EXPECT_EQ(metric->name(), name);
            EXPECT_EQ(metric->labelsString(), labelsString);
        }
    }
}

struct MetricsFamilyTest : ::testing::Test {
    struct MetricMock : MetricBase {
        using MetricBase::MetricBase;
        MOCK_METHOD(void, serializeValue, (std::string&), (const));
    };
    using MetricStrictMock = ::testing::StrictMock<MetricMock>;

    struct MetricBuilderImplMock {
        MOCK_METHOD(std::unique_ptr<MetricBase>, build, (std::string, std::string, MetricType));
    };

    ::testing::StrictMock<MetricBuilderImplMock> metricBuilderMock;
    MetricsFamily::MetricBuilder metricBuilder =
        [this](std::string metricName, std::string labels, MetricType metricType) {
            return metricBuilderMock.build(std::move(metricName), std::move(labels), metricType);
        };

    std::string const name{"name"};
    std::string const description{"description"};
    MetricType const type{MetricType::COUNTER_INT};
    MetricsFamily metricsFamily{name, description, type, metricBuilder};
};

TEST_F(MetricsFamilyTest, getters)
{
    EXPECT_EQ(metricsFamily.name(), name);
    EXPECT_EQ(metricsFamily.type(), type);
}

TEST_F(MetricsFamilyTest, getMetric)
{
    Labels const labels{{{"label1", "value1"}}};
    std::string const labelsString = labels.serialize();

    EXPECT_CALL(metricBuilderMock, build(name, labelsString, type))
        .WillOnce(::testing::Return(std::make_unique<MetricStrictMock>(name, labelsString)));

    auto& metric = metricsFamily.getMetric(labels);
    EXPECT_EQ(metric.name(), name);
    EXPECT_EQ(metric.labelsString(), labelsString);

    auto* metricMock = dynamic_cast<MetricStrictMock*>(&metric);
    ASSERT_NE(metricMock, nullptr);
    EXPECT_EQ(&metricsFamily.getMetric(labels), &metric);

    Labels const labels2{{{"label1", "value2"}}};
    std::string const labels2String = labels2.serialize();

    EXPECT_CALL(metricBuilderMock, build(name, labels2String, type))
        .WillOnce(::testing::Return(std::make_unique<MetricStrictMock>(name, labels2String)));

    auto& metric2 = metricsFamily.getMetric(labels2);
    EXPECT_EQ(metric2.name(), name);
    EXPECT_EQ(metric2.labelsString(), labels2String);

    auto* metric2Mock = dynamic_cast<MetricStrictMock*>(&metric2);
    ASSERT_NE(metric2Mock, nullptr);
    EXPECT_EQ(&metricsFamily.getMetric(labels2), &metric2);
    EXPECT_NE(&metric, &metric2);

    EXPECT_CALL(*metricMock, serializeValue(::testing::_)).WillOnce([](std::string& s) { s += "metric"; });
    EXPECT_CALL(*metric2Mock, serializeValue(::testing::_)).WillOnce([](std::string& s) { s += "metric2"; });

    std::string serialized;
    metricsFamily.serialize(serialized);

    auto const expected =
        fmt::format("# HELP {0} {1}\n# TYPE {0} {2}\nmetric\nmetric2\n\n", name, description, toString(type));
    auto const anotherExpected =
        fmt::format("# HELP {0} {1}\n# TYPE {0} {2}\nmetric2\nmetric\n\n", name, description, toString(type));
    EXPECT_TRUE(serialized == expected || serialized == anotherExpected);
}
