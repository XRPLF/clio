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

#include "util/prometheus/Label.hpp"
#include "util/prometheus/MetricBase.hpp"
#include "util/prometheus/MetricBuilder.hpp"
#include "util/prometheus/MetricsFamily.hpp"
#include "util/prometheus/OStream.hpp"

#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace util::prometheus;

struct MetricsFamilyTest : ::testing::Test {
    struct MetricMock : MetricBase {
        using MetricBase::MetricBase;
        MOCK_METHOD(void, serializeValue, (OStream&), (const));
    };
    using MetricStrictMock = ::testing::StrictMock<MetricMock>;

    struct MetricBuilderImplMock : MetricBuilderInterface {
        std::unique_ptr<MetricBase>
        operator()(
            std::string metricName,
            std::string labelsString,
            MetricType metricType,
            std::vector<std::int64_t> const& buckets
        ) override
        {
            return buildInt(std::move(metricName), std::move(labelsString), metricType, buckets);
        }

        std::unique_ptr<MetricBase>
        operator()(
            std::string metricName,
            std::string labelsString,
            MetricType metricType,
            std::vector<double> const& buckets
        ) override
        {
            return buildDouble(std::move(metricName), std::move(labelsString), metricType, buckets);
        }

        MOCK_METHOD(
            std::unique_ptr<MetricBase>,
            buildInt,
            (std::string, std::string, MetricType, std::vector<std::int64_t> const&)
        );
        MOCK_METHOD(
            std::unique_ptr<MetricBase>,
            buildDouble,
            (std::string, std::string, MetricType, std::vector<double> const&)
        );
    };

    ::testing::StrictMock<MetricBuilderImplMock> metricBuilderMock;

    std::string const name{"name"};
    std::string const description{"description"};
    MetricType const type{MetricType::COUNTER_INT};
    MetricsFamily metricsFamily{name, description, type, metricBuilderMock};
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

    EXPECT_CALL(metricBuilderMock, buildInt(name, labelsString, type, std::vector<std::int64_t>{}))
        .WillOnce(::testing::Return(std::make_unique<MetricStrictMock>(name, labelsString)));

    auto& metric = metricsFamily.getMetric(labels);
    EXPECT_EQ(metric.name(), name);
    EXPECT_EQ(metric.labelsString(), labelsString);

    auto* metricMock = dynamic_cast<MetricStrictMock*>(&metric);
    ASSERT_NE(metricMock, nullptr);
    EXPECT_EQ(&metricsFamily.getMetric(labels), &metric);

    Labels const labels2{{{"label1", "value2"}}};
    std::string const labels2String = labels2.serialize();

    EXPECT_CALL(metricBuilderMock, buildInt(name, labels2String, type, std::vector<std::int64_t>{}))
        .WillOnce(::testing::Return(std::make_unique<MetricStrictMock>(name, labels2String)));

    auto& metric2 = metricsFamily.getMetric(labels2);
    EXPECT_EQ(metric2.name(), name);
    EXPECT_EQ(metric2.labelsString(), labels2String);

    auto* metric2Mock = dynamic_cast<MetricStrictMock*>(&metric2);
    ASSERT_NE(metric2Mock, nullptr);
    EXPECT_EQ(&metricsFamily.getMetric(labels2), &metric2);
    EXPECT_NE(&metric, &metric2);

    EXPECT_CALL(*metricMock, serializeValue(::testing::_)).WillOnce([](OStream& s) { s << "metric"; });
    EXPECT_CALL(*metric2Mock, serializeValue(::testing::_)).WillOnce([](OStream& s) { s << "metric2"; });

    OStream stream{false};
    stream << metricsFamily;
    auto const serialized = std::move(stream).data();

    auto const expected =
        fmt::format("# HELP {0} {1}\n# TYPE {0} {2}\nmetric\nmetric2\n\n", name, description, toString(type));
    auto const anotherExpected =
        fmt::format("# HELP {0} {1}\n# TYPE {0} {2}\nmetric2\nmetric\n\n", name, description, toString(type));
    EXPECT_TRUE(serialized == expected || serialized == anotherExpected);
}
