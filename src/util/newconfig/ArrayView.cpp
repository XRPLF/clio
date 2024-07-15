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

#include "util/newconfig/ArrayView.hpp"

#include "util/Assert.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/ValueView.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace util::config {

ArrayView::ArrayView(std::string_view prefix, ClioConfigDefinition const& configDef)
    : prefix_{prefix}, clioConfig_{configDef}
{
    auto it = std::find_if(configDef.map_.begin(), configDef.map_.end(), [this](auto const& pair) {
        return pair.first.starts_with(prefix_);
    });
    ASSERT(it != clioConfig_.map_.end(), "prefix does not exist in config definition.");
    ASSERT(prefix_.contains(".[]"), "Not an array");
}

ValueView
ArrayView::valueAt(std::size_t idx) const
{
    ASSERT(clioConfig_.map_.contains(prefix_), "Current string is prefix, not full key");
    ConfigValue const& val = std::get<Array>(clioConfig_.map_.at(prefix_)).at(idx);
    return ValueView{val};
}

size_t
ArrayView::size() const
{
    for (auto const& pair : clioConfig_.map_) {
        if (pair.first.starts_with(prefix_)) {
            return std::get<Array>(pair.second).size();
        }
    }
    throw std::logic_error("Arrayview is initialized with incorrect prefix.");
}

ObjectView
ArrayView::objectAt(std::size_t idx) const
{
    ASSERT(idx < this->size(), "Object index is out of scope");
    return ObjectView{prefix_, idx, clioConfig_};
}

}  // namespace util::config
