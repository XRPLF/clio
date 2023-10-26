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

#include <util/prometheus/Prometheus.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace util::prometheus {

template <impl::SomeNumberType NumberType>
struct MockCounterImpl {
    using ValueType = NumberType;

    MOCK_METHOD(void, add, (NumberType), ());
    MOCK_METHOD(void, set, (NumberType), ());
    MOCK_METHOD(NumberType, value, (), ());
};

using MockCounterImplInt = MockCounterImpl<std::int64_t>;
using MockCounterImplUint = MockCounterImpl<std::uint64_t>;
using MockCounterImplDouble = MockCounterImpl<double>;

struct MockPrometheusImpl : PrometheusInterface {
    MockPrometheusImpl() : PrometheusInterface(true)
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
    }

    MOCK_METHOD(CounterInt&, counterInt, (std::string, Labels, std::optional<std::string>), (override));
    MOCK_METHOD(CounterDouble&, counterDouble, (std::string, Labels, std::optional<std::string>), (override));
    MOCK_METHOD(GaugeInt&, gaugeInt, (std::string, Labels, std::optional<std::string>), (override));
    MOCK_METHOD(GaugeDouble&, gaugeDouble, (std::string, Labels, std::optional<std::string>), (override));
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
        if (metricPtr == nullptr)
            throw std::runtime_error("Wrong metric type");
        return *metricPtr;
    }

    template <typename MetricType>
    MetricType&
    makeMetric(std::string name, std::string labelsString)
    {
        std::unique_ptr<MetricBase> metric;
        auto const key = name + labelsString;
        if constexpr (std::is_same_v<typename MetricType::ValueType, std::int64_t>) {
            auto& impl = counterIntImpls[key];
            metric = std::make_unique<MetricType>(name, labelsString, impl);
        } else if constexpr (std::is_same_v<typename MetricType::ValueType, std::uint64_t>) {
            auto& impl = counterUintImpls[key];
            metric = std::make_unique<MetricType>(name, labelsString, impl);
        } else {
            auto& impl = counterDoubleImpls[key];
            metric = std::make_unique<MetricType>(name, labelsString, impl);
        }
        auto* ptr = metrics.emplace(key, std::move(metric)).first->second.get();
        auto metricPtr = dynamic_cast<MetricType*>(ptr);
        if (metricPtr == nullptr)
            throw std::runtime_error("Wrong metric type");
        return *metricPtr;
    }

    std::unordered_map<std::string, std::unique_ptr<MetricBase>> metrics;
    std::unordered_map<std::string, ::testing::StrictMock<MockCounterImplInt>> counterIntImpls;
    std::unordered_map<std::string, ::testing::StrictMock<MockCounterImplUint>> counterUintImpls;
    std::unordered_map<std::string, ::testing::StrictMock<MockCounterImplDouble>> counterDoubleImpls;
};

/**
 * @note this class should be the first in the inheritance list
 */
struct WithMockPrometheus : virtual ::testing::Test {
    WithMockPrometheus()
    {
        PrometheusSingleton::replaceInstance(std::make_unique<MockPrometheusImpl>());
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
        PROMETHEUS_INIT();
    }

    static MockPrometheusImpl&
    mockPrometheus()
    {
        auto* ptr = dynamic_cast<MockPrometheusImpl*>(&PROMETHEUS());
        if (ptr == nullptr)
            throw std::runtime_error("Wrong prometheus type");
        return *ptr;
    }

    template <typename MetricType>
    static auto&
    makeMock(std::string name, std::string labelsString)
    {
        auto* mockPrometheusPtr = dynamic_cast<MockPrometheusImpl*>(&PROMETHEUS());
        if (mockPrometheusPtr == nullptr)
            throw std::runtime_error("Wrong prometheus type");

        std::string const key = name + labelsString;
        mockPrometheusPtr->makeMetric<MetricType>(std::move(name), std::move(labelsString));
        if constexpr (std::is_same_v<typename MetricType::ValueType, std::int64_t>) {
            return mockPrometheusPtr->counterIntImpls[key];
        } else if constexpr (std::is_same_v<typename MetricType::ValueType, std::uint64_t>) {
            return mockPrometheusPtr->counterUintImpls[key];
        } else if constexpr (std::is_same_v<typename MetricType::ValueType, double>) {
            return mockPrometheusPtr->counterDoubleImpls[key];
        }
        throw std::runtime_error("Wrong metric type");
    }
};

/**
 * @note this class should be the first in the inheritance list
 */
struct WithPrometheus : virtual ::testing::Test {
    WithPrometheus()
    {
        PROMETHEUS_INIT();
    }

    ~WithPrometheus() override
    {
        PROMETHEUS_INIT();
    }
};

}  // namespace util::prometheus
