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

#include <util/prometheus/Label.h>

#include <fmt/format.h>

namespace util::prometheus {

Label::Label(std::string name_, std::string value_) : name_(std::move(name_)), value_(std::move(value_))
{
}

bool
Label::operator<(const Label& rhs) const
{
    return std::tie(name_, value_) < std::tie(rhs.name_, rhs.value_);
}

bool
Label::operator==(const Label& rhs) const
{
    return std::tie(name_, value_) == std::tie(rhs.name_, rhs.value_);
}

std::string
Label::serialize() const
{
    std::string escapedValue;
    escapedValue.reserve(value_.size());
    for (const auto c : value_)
    {
        switch (c)
        {
            case '\n': {
                escapedValue.push_back('\\');
                escapedValue.push_back('n');
                break;
            }
            case '\\':
                [[fallthrough]];
            case '"': {
                escapedValue.push_back('\\');
                [[fallthrough]];
            }
            default:
                escapedValue.push_back(c);
                break;
        }
    }
    return fmt::format("{}=\"{}\"", name_, value_);
}

}  // namespace util::prometheus
