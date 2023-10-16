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

#include <util/prometheus/Label.h>

#include <fmt/format.h>

#include <unordered_map>

namespace util::prometheus {

class MetricBase
{
public:
    MetricBase(std::string name, std::string labelsString);

    MetricBase(const MetricBase&) = delete;
    MetricBase(MetricBase&&) = default;
    MetricBase&
    operator=(const MetricBase&) = delete;
    MetricBase&
    operator=(MetricBase&&) = default;
    virtual ~MetricBase() = default;

    void
    serialize(std::string& s) const;

    const std::string&
    name() const;

    const std::string&
    labelsString() const;

protected:
    virtual void
    serializeValue(std::string& result) const = 0;

private:
    std::string name_;
    std::string labelsString_;
};

enum class MetricType { COUNTER_INT, COUNTER_DOUBLE, GAUGE_INT, GAUGE_DOUBLE, HISTOGRAM, SUMMARY };

const char*
toString(MetricType type);

template <typename T>
concept SomeMetric = std::is_base_of_v<T, MetricBase>;

class MetricsFamily
{
public:
    MetricsFamily(std::string name, std::optional<std::string> description, MetricType type);

    MetricsFamily(const MetricsFamily&) = delete;
    MetricsFamily(MetricsFamily&&) = default;
    MetricsFamily&
    operator=(const MetricsFamily&) = delete;
    MetricsFamily&
    operator=(MetricsFamily&&) = default;

    MetricBase&
    getMetric(Labels labels);

    void
    serialize(std::string& result) const;

    const std::string&
    name() const;

    MetricType
    type() const;

private:
    std::string name_;
    std::optional<std::string> description_;
    std::unordered_map<std::string, std::unique_ptr<MetricBase>> metrics_;
    MetricType type_;
};

}  // namespace util::prometheus
