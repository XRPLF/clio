//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/newconfig/ValueView.hpp"

#include "util/Assert.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"

#include <string>
#include <string_view>
#include <variant>

namespace util::config {

std::string_view
ValueView::asString() const
{
    if (this->type() == ConfigType::String)
        return std::get<std::string>(configVal_.value_.value());
    throw std::bad_variant_access();
}

bool
ValueView::asBool() const
{
    if (type() == ConfigType::Boolean && configVal_.value_.has_value())
        return std::get<bool>(configVal_.value_.value());
    throw std::bad_variant_access();
}

int
ValueView::asInt() const
{
    if (type() == ConfigType::Integer && configVal_.value_.has_value())
        return std::get<int>(configVal_.value_.value());
    throw std::bad_variant_access();
}

double
ValueView::asDouble() const
{
    if (type() == ConfigType::Double && configVal_.value_.has_value())
        return std::get<double>(configVal_.value_.value());
    throw std::bad_variant_access();
}

}  // namespace util::config
