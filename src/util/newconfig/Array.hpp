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

#pragma once

#include "util/Assert.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/ValueView.hpp"

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

namespace util::config {

/**
 * @brief Array definition for Json/Yaml config
 *
 * Used in ClioConfigDefinition to represent multiple potential values (like whitelist)
 */
class Array {
public:
    /**
     * @brief Constructs an Array with the provided arguments
     *
     * @tparam Args Types of the arguments
     * @param args Arguments to initialize the elements of the Array
     */
    template <typename... Args>
    constexpr Array(Args&&... args) : elements_{std::forward<Args>(args)...}
    {
    }

    /**
     * @brief Add ConfigValues to Array class
     *
     * @param value The ConfigValue to add
     */
    void
    emplace_back(ConfigValue value)
    {
        elements_.push_back(std::move(value));
    }

    /**
     * @brief Returns the number of values stored in the Array
     *
     * @return Number of values stored in the Array
     */
    size_t
    size() const
    {
        return elements_.size();
    }

    /**
     * @brief Returns the ConfigValue at the specified index
     *
     * @param idx Index of the ConfigValue to retrieve
     * @return ConfigValue at the specified index
     */
    ConfigValue const&
    at(std::size_t idx) const
    {
        ASSERT(idx < elements_.size(), "index is out of scope");
        return elements_[idx];
    }

private:
    std::vector<ConfigValue> elements_;
};

}  // namespace util::config
