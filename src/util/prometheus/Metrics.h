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

namespace util::prometheus {

class MetricBase
{
public:
    MetricBase(std::string name, Labels labels, std::optional<std::string> description);

    MetricBase(const MetricBase&) = delete;
    MetricBase(MetricBase&&) = default;
    MetricBase&
    operator=(const MetricBase&) = delete;
    MetricBase&
    operator=(MetricBase&&) = default;
    virtual ~MetricBase() = default;

    std::string
    serialize() const;

    const std::string&
    key() const;

protected:
    virtual const char*
    type() const = 0;
    virtual void
    serializeValue(std::string& result) const = 0;

private:
    std::string name_;
    std::string key_;
    std::optional<std::string> description_;
};

// template <SomeNumberType ImplType>
// class Gauge : public Counter<ImplType>
// {
// public:
//     template <SomeNumberType OtherType>
//     void
//     operator+=(OtherType value)
//     {
//         value_ += value;
//     }
//
//     Gauge&
//     operator--()
//     {
//         --value_;
//         return *this;
//     }
//
//     template <SomeNumberType OtherType>
//     void
//     operator-=(OtherType value)
//     {
//         value_ -= value;
//     }
//
// protected:
//     const char*
//     type() const override
//     {
//         return "gauge";
//     }
// };
// using GaugeInt = Gauge<std::int64_t>;
// using GaugeDouble = Gauge<double>;

}  // namespace util::prometheus
