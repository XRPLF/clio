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
#include <util/prometheus/Metrics.h>

namespace util::prometheus {

MetricBase::MetricBase(std::string name, Labels labels, std::optional<std::string> description)
    : name_(std::move(name)), key_({}), description_(std::move(description))
{
    fmt::format_to(std::back_inserter(key_), "{}{{", name_);
    std::sort(labels.begin(), labels.end());
    for (auto it = labels.begin(); it != labels.end(); ++it)
    {
        if (it != labels.begin())
            fmt::format_to(std::back_inserter(key_), ",");
        fmt::format_to(std::back_inserter(key_), "{}", it->serialize());
    }
    key_ += "}";
}

std::string
MetricBase::serialize() const
{
    std::string result;
    if (description_)
        fmt::format_to(std::back_inserter(result), "# HELP {} {}\n", name_, *description_);
    fmt::format_to(std::back_inserter(result), "# TYPE {} {}\n", name_, type());
    serializeValue(result);
    return result;
}

const std::string&
MetricBase::key() const
{
    return key_;
}

}  // namespace util::prometheus
