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

#include <cstdint>
#include <string_view>
#include <variant>

namespace util::config {

/**
 * @brief A class to enforce constraints on certain values within ClioConfigDefinition
 * TODO: will be used when parsing user config
 */
class ConfigConstraints {
public:
    /**
     * @brief make sure port is in a valid range
     *
     * @param port port to check valid range. Clio config uses both string and int
     * @return true if port is within a valid range, false otherwise
     */
    bool
    checkPortRange(std::variant<std::string_view, int> port) const;

    /**
     * @brief Check if all the log channel's value is correct
     *
     * @return if all log channel values are correct, false otherwise
     */
    bool
    checkLogChannels() const;

private:
    uint32_t const portMin = 1;
    uint32_t const portMax = 65535;
};

}  // namespace util::config
