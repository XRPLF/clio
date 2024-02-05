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

#include "util/Assert.hpp"
#include "util/config/Config.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Histogram.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/MetricBase.hpp"
#include "util/prometheus/OStream.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace util::prometheus {

template <SomeNumberType NumberType>
struct MockCounterImpl {
    using ValueType = NumberType;

    MOCK_METHOD(void, add, (NumberType), ());
    MOCK_METHOD(void, set, (NumberType), ());
    MOCK_METHOD(NumberType, value, (), ());
};

using MockCounterImplInt = MockCounterImpl<std::int64_t>;
using MockCounterImplUint = MockCounterImpl<std::uint64_t>;
using MockCounterImplDouble = MockCounterImpl<double>;

template <typename NumberType>
    requires std::same_as<NumberType, std::int64_t> || std::same_as<NumberType, double>
struct MockHistogramImpl {
    using ValueType = NumberType;

    MockHistogramImpl()
    {
        EXPECT_CALL(*this, setBuckets);
    }

    MOCK_METHOD(void, observe, (ValueType), ());
    MOCK_METHOD(void, setBuckets, (std::vector<ValueType> const&), ());
    MOCK_METHOD(void, serializeValue, (std::string const&, std::string, OStream&), (const));
};

using MockHistogramImplInt = MockHistogramImpl<std::int64_t>;
using MockHistogramImplDouble = MockHistogramImpl<double>;

struct MockPrometheusImpl : PrometheusInterface {
    MockPrometheusImpl() : PrometheusInterface(true, true)
    {
        EXPECT_CALL(*this, counterInt)
            .WillRepeatedly([this](std::string name, Labels labels, std::optional<std::string>) -> CounterInt& {
                return getMetric<CounterInt>(std::move(name), std::move(labels));
            });
        EXPECT_CALL(*this, counterDouble)
            .WillRepeatedly([this](std::string name, Labels labels, std::optional<std::string>) -> CounterDouble& {
                return getMetric<CounterDouble>(std::move(name), std::move(labels));
            });
        EXPECT_CALL(*this, gaugeInt)
            .WillRepeatedly([this](std::string name, Labels labels, std::optional<std::string>) -> GaugeInt& {
                return getMetric<GaugeInt>(std::move(name), std::move(labels));
            });
        EXPECT_CALL(*this, gaugeDouble)
            .WillRepeatedly([this](std::string name, Labels labels, std::optional<std::string>) -> GaugeDouble& {
                return getMetric<GaugeDouble>(std::move(name), std::move(labels));
            });
        EXPECT_CALL(*this, histogramInt)
            .WillRepeatedly(
                [this](std::string name, Labels labels, std::vector<std::int64_t> const&, std::optional<std::string>)
                    -> HistogramInt& { return getMetric<HistogramInt>(std::move(name), std::move(labels)); }
            );
        EXPECT_CALL(*this, histogramDouble)
            .WillRepeatedly(
                [this](std::string name, Labels labels, std::vector<double> const&, std::optional<std::string>)
                    -> HistogramDouble& { return getMetric<HistogramDouble>(std::move(name), std::move(labels)); }
            );
    }

    MOCK_METHOD(CounterInt&, counterInt, (std::string, Labels, std::optional<std::string>), (override));
    MOCK_METHOD(CounterDouble&, counterDouble, (std::string, Labels, std::optional<std::string>), (override));
    MOCK_METHOD(GaugeInt&, gaugeInt, (std::string, Labels, std::optional<std::string>), (override));
    MOCK_METHOD(GaugeDouble&, gaugeDouble, (std::string, Labels, std::optional<std::string>), (override));
    MOCK_METHOD(
        HistogramInt&,
        histogramInt,
        (std::string, Labels, std::vector<std::int64_t> const&, std::optional<std::string>),
        (override)
    );
    MOCK_METHOD(
        HistogramDouble&,
        histogramDouble,
        (std::string, Labels, std::vector<double> const&, std::optional<std::string>),
        (override)
    );
    MOCK_METHOD(std::string, collectMetrics, (), (override));

    template <typename MetricType>
    MetricType&
    getMetric(std::string name, Labels labels)
    {
        auto const labelsString = labels.serialize();
        auto const key = name + labels.serialize();
        auto it = metrics.find(key);
        if (it == metrics.end()) {
            return makeMetric<MetricType>(std::move(name), labels.serialize());
        }
        auto* basePtr = it->second.get();
        auto* metricPtr = dynamic_cast<MetricType*>(basePtr);
        ASSERT(metricPtr != nullptr, "Wrong metric type");
        return *metricPtr;
    }

    template <typename MetricType>
    MetricType&
    makeMetric(std::string name, std::string labelsString)
    {
        std::unique_ptr<MetricBase> metric;
        auto const key = name + labelsString;
        if constexpr (std::is_same_v<MetricType, GaugeInt>) {
            auto& impl = counterIntImpls[key];
            metric = std::make_unique<MetricType>(name, labelsString, impl);
        } else if constexpr (std::is_same_v<MetricType, CounterInt>) {
            auto& impl = counterUintImpls[key];
            metric = std::make_unique<MetricType>(name, labelsString, impl);
        } else if constexpr (std::is_same_v<MetricType, GaugeDouble> || std::is_same_v<MetricType, CounterDouble>) {
            auto& impl = counterDoubleImpls[key];
            metric = std::make_unique<MetricType>(name, labelsString, impl);
        } else if constexpr (std::is_same_v<MetricType, HistogramInt>) {
            auto& impl = histogramIntImpls[key];
            metric = std::make_unique<MetricType>(name, labelsString, std::vector<std::int64_t>{1}, impl);
        } else if constexpr (std::is_same_v<MetricType, HistogramDouble>) {
            auto& impl = histogramDoubleImpls[key];
            metric = std::make_unique<MetricType>(name, labelsString, std::vector<double>{1.}, impl);
        } else {
            throw std::runtime_error("Wrong metric type");
        }
        auto* ptr = metrics.emplace(key, std::move(metric)).first->second.get();
        auto metricPtr = dynamic_cast<MetricType*>(ptr);
        ASSERT(metricPtr != nullptr, "Wrong metric type");
        return *metricPtr;
    }

    std::unordered_map<std::string, std::unique_ptr<MetricBase>> metrics;
    std::unordered_map<std::string, ::testing::StrictMock<MockCounterImplInt>> counterIntImpls;
    std::unordered_map<std::string, ::testing::StrictMock<MockCounterImplUint>> counterUintImpls;
    std::unordered_map<std::string, ::testing::StrictMock<MockCounterImplDouble>> counterDoubleImpls;
    std::unordered_map<std::string, ::testing::StrictMock<MockHistogramImplInt>> histogramIntImpls;
    std::unordered_map<std::string, ::testing::StrictMock<MockHistogramImplDouble>> histogramDoubleImpls;
};

/**
 * @note this class should be the first in the inheritance list
 */
struct WithMockPrometheus : virtual ::testing::Test {
    WithMockPrometheus()
    {
        PrometheusService::replaceInstance(std::make_unique<MockPrometheusImpl>());
    }

    ~WithMockPrometheus() override
    {
        if (HasFailure()) {
            std::cerr << "Registered metrics:\n";
            for (auto const& [key, metric] : mockPrometheus().metrics) {
                std::cerr << key << "\n";
            }
            std::cerr << "\n";
        }
        PrometheusService::init();
    }

    static MockPrometheusImpl&
    mockPrometheus()
    {
        auto* ptr = dynamic_cast<MockPrometheusImpl*>(&PrometheusService::instance());
        ASSERT(ptr != nullptr, "Wrong prometheus type");
        return *ptr;
    }

    template <typename MetricType>
    static auto&
    makeMock(std::string name, std::string labelsString)
    {
        auto* mockPrometheusPtr = dynamic_cast<MockPrometheusImpl*>(&PrometheusService::instance());
        ASSERT(mockPrometheusPtr != nullptr, "Wrong prometheus type");

        std::string const key = name + labelsString;

        if (!mockPrometheusPtr->metrics.contains(key))
            mockPrometheusPtr->makeMetric<MetricType>(std::move(name), std::move(labelsString));

        if constexpr (std::is_same_v<MetricType, GaugeInt>) {
            return mockPrometheusPtr->counterIntImpls[key];
        } else if constexpr (std::is_same_v<MetricType, CounterInt>) {
            return mockPrometheusPtr->counterUintImpls[key];
        } else if constexpr (std::is_same_v<MetricType, GaugeDouble> || std::is_same_v<MetricType, CounterDouble>) {
            return mockPrometheusPtr->counterDoubleImpls[key];
        } else if constexpr (std::is_same_v<MetricType, HistogramInt>) {
            return mockPrometheusPtr->histogramIntImpls[key];
        } else if constexpr (std::is_same_v<MetricType, HistogramDouble>) {
            return mockPrometheusPtr->histogramDoubleImpls[key];
        }
        ASSERT(false, "Wrong metric type for metric {} {}", name, labelsString);

        // to fix -Werror=return-type for gcc 14.1 in Debug mode
        throw std::runtime_error("Wrong metric type");
    }
};

/**
 * @note this class should be the first in the inheritance list
 */
struct WithPrometheus : virtual ::testing::Test {
    WithPrometheus()
    {
        boost::json::value const config{{"prometheus", boost::json::object{{"compress_reply", false}}}};
        PrometheusService::init(Config{config});
    }

    ~WithPrometheus() override
    {
        PrometheusService::init();
    }
};

}  // namespace util::prometheus
