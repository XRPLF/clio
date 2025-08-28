//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include <xrpl/basics/base_uint.h>

#include <string>
#include <string_view>

namespace util {
/**
 * @brief Calculates the SHA256 sum of a string.
 *
 * @param s The string to hash.
 * @return The SHA256 sum as a ripple::uint256.
 */
ripple::uint256
sha256sum(std::string_view s);

/**
 * @brief Calculates the SHA256 sum of a string and returns it as a hex string.
 *
 * @param s The string to hash.
 * @return The SHA256 sum as a hex string.
 */
std::string
sha256sumString(std::string_view s);

}  // namespace util
