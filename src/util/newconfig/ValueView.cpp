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
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

namespace util::config {

ValueView::ValueView(ConfigValue const& configVal) : configVal_{configVal}
{
}

std::string
ValueView::asString() const
{
    if (this->type() == ConfigType::String && configVal_.get().hasValue())
        return std::get<std::string>(configVal_.get().getValue());
    ASSERT(false, "Value view is not of String type");
    std::unreachable();
}

bool
ValueView::asBool() const
{
    if (type() == ConfigType::Boolean && configVal_.get().hasValue())
        return std::get<bool>(configVal_.get().getValue());
    ASSERT(false, "Value view is not of Bool type");
    std::unreachable();
}

double
ValueView::asDouble() const
{
    if (configVal_.get().hasValue()) {
        auto const& val = configVal_.get().getValue();

        if (type() == ConfigType::Double) {
            if (std::holds_alternative<int64_t>(val)) {
                return static_cast<double>(std::get<int64_t>(val));
            }
            return static_cast<double>(std::get<double>(val));
        }
        if (type() == ConfigType::Integer)
            return static_cast<double>(std::get<int64_t>(configVal_.get().getValue()));
    }
    ASSERT(false, "Value view is not of Double type");
    std::unreachable();
}

float
ValueView::asFloat() const
{
    if (configVal_.get().hasValue()) {
        auto const& val = configVal_.get().getValue();

        if (type() == ConfigType::Double) {
            if (std::holds_alternative<int64_t>(val)) {
                return static_cast<float>(std::get<int64_t>(val));
            }
            return static_cast<float>(std::get<double>(val));
        }
        if (type() == ConfigType::Integer)
            return static_cast<float>(std::get<int64_t>(val));
    }
    ASSERT(false, "Value view is not of Float type");
    std::unreachable();
}

}  // namespace util::config
