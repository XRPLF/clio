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

#include "util/newconfig/Array.hpp"

#include "util/Assert.hpp"
#include "util/newconfig/ConfigValue.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace util::config {

void
Array::emplaceBack(ConfigValue value)
{
    ASSERT(value.type() == elements_.front().type(), "Trying to insert a Value of Wrong Type");
    if (!elements_.front().hasValue()) {
        elements_.front() = std::move(value);
    } else {
        elements_.push_back(std::move(value));
    }
}

size_t
Array::size() const
{
    return elements_.size();
}

ConfigValue const&
Array::at(std::size_t idx) const
{
    ASSERT(idx < elements_.size(), "Index is out of scope");
    return elements_[idx];
}

std::vector<ConfigValue>::const_iterator
Array::begin() const
{
    return elements_.begin();
}

std::vector<ConfigValue>::const_iterator
Array::end() const
{
    return elements_.end();
}

}  // namespace util::config
