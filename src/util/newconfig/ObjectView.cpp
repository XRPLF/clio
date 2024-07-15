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

#include "util/newconfig/ObjectView.hpp"

#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ValueView.hpp"

#include <fmt/core.h>
#include <unistd.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace util::config {

ObjectView::ObjectView(std::string_view prefix, ClioConfigDefinition const& clioConfig)
    : prefix_{prefix}, clioConfig_{clioConfig}
{
}

ObjectView::ObjectView(std::string_view prefix, std::size_t arrayIndex, ClioConfigDefinition const& clioConfig)
    : prefix_{prefix}, arrayIndex_{arrayIndex}, clioConfig_{clioConfig}
{
}

std::string
ObjectView::getFullKey(std::string_view key) const
{
    return fmt::format("{}.{}", prefix_, key);
}

bool
ObjectView::containsKey(std::string_view key) const
{
    std::string fullKey = getFullKey(key);
    return clioConfig_.map_.contains(fullKey);
}

bool
ObjectView::startsWithKey(std::string_view key) const
{
    for (auto const& [valueKey, value] : clioConfig_.map_) {
        if (valueKey.starts_with(key))
            return true;
    }
    return false;
}

ValueView
ObjectView::getValue(std::string_view key) const
{
    std::string fullKey = getFullKey(key);
    if (arrayIndex_.has_value()) {
        return clioConfig_.getArray(fullKey).valueAt(arrayIndex_.value());
    }
    return clioConfig_.getValue(fullKey);
}

ObjectView
ObjectView::getObject(std::string_view key) const
{
    std::string fullKey = getFullKey(key);
    if (startsWithKey(fullKey) && !arrayIndex_.has_value()) {
        return clioConfig_.getObject(fullKey);
    }
    if (startsWithKey(fullKey) && arrayIndex_.has_value()) {
        return ObjectView(fullKey, arrayIndex_.value(), clioConfig_);
    }
    throw std::runtime_error("Key does not exist in object");
}

}  // namespace util::config
