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

#include "util/newconfig/ConfigValue.hpp"

#include <cassert>
#include <cstddef>
#include <string_view>

namespace util::config {

class ClioConfigDefinition;

/**
 * @brief Provides view into ConfigValues that represents values in Clio Config
 */
class ValueView {
public:
    /**
     * @brief Constructs a ValueView object
     *
     * @param configVal the config Value to view
     */
    ValueView(ConfigValue const& configVal) : configVal_{configVal}
    {
    }

    /**
     * @brief Retrieves the value as a string
     *
     * @return The value as a string
     * @throws std::bad_variant_access if the value is not a string
     */
    std::string_view
    asString() const;

    /**
     * @brief Retrieves the value as a boolean
     *
     * @return The value as a boolean
     * @throws std::bad_variant_access if the value is not a boolean
     */
    bool
    asBool() const;

    /**
     * @brief Retrieves the value as an integer
     *
     * @return The value as an integer
     * @throws std::bad_variant_access if the value is not an integer
     */
    int
    asInt() const;

    /**
     * @brief Retrieves the value as a double
     *
     * @return The value as a double
     * @throws std::bad_variant_access if the value is not a double
     */
    double
    asDouble() const;

    /**
     * @brief Gets the config type
     *
     * @return The config type
     */
    constexpr ConfigType
    type() const
    {
        return configVal_.type();
    }

private:
    ConfigValue const& configVal_;
};

}  // namespace util::config
