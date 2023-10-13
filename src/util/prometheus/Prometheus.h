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

#include <cassert>
#include <util/prometheus/Counter.h>

namespace util::prometheus {

class PrometheusInterface
{
public:
    virtual ~PrometheusInterface() = default;

    virtual CounterInt&
    counterInt(std::string name, Labels labels, std::optional<std::string> description) = 0;
    virtual CounterDouble&
    counterDouble(std::string name, Labels labels, std::optional<std::string> description) = 0;

    // virtual GaugeInt&
    // gaugeInt(std::string name, Labels labels, std::optional<std::string> description) = 0;
    // virtual GaugeDouble&
    // gaugeDouble(std::string name, Labels labels, std::optional<std::string> description) = 0;

    virtual std::string
    collectMetrics() = 0;
};

class PrometheusImpl : public PrometheusInterface
{
public:
    CounterInt&
    counterInt(std::string name, Labels labels, std::optional<std::string> description) override;
    CounterDouble&
    counterDouble(std::string name, Labels labels, std::optional<std::string> description) override;

    // GaugeInt&
    // gaugeInt(std::string name, Labels labels, std::optional<std::string> description) override;
    // GaugeDouble&
    // gaugeDouble(std::string name, Labels labels, std::optional<std::string> description) override;

    std::string
    collectMetrics() override;

private:
    std::unordered_map<std::string, std::unique_ptr<MetricBase>> metrics_;
};

class PrometheusSingleton
{
public:
    void static init()
    {
        instance_ = std::make_unique<PrometheusImpl>();
    }

    static PrometheusInterface&
    instance()
    {
        assert(instance_);
        return *instance_;
    }

    static void
    replaceInstance(std::unique_ptr<PrometheusInterface> instance)
    {
        instance_ = std::move(instance);
    }

private:
    static std::unique_ptr<PrometheusInterface> instance_;
};

}  // namespace util::prometheus
