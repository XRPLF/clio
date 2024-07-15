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

#include <xrpl/protocol/tokens.h>

#include <cctype>
#include <optional>
#include <string>

namespace util {

/**
 * @brief A wrapper of parseBase58 function. It adds the check if all characters in the input string are alphanumeric.
 *If not, it returns an empty optional, instead of calling the parseBase58 function.
 *
 *@tparam T The type of the value to parse to.
 *@param str The string to parse.
 *@return An optional with the parsed value, or an empty optional if the parse fails.
 */
template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(std::string const& str)
{
    if (!std::all_of(std::begin(str), std::end(str), [](unsigned char c) { return std::isalnum(c); }))
        return std::nullopt;

    return ripple::parseBase58<T>(str);
}

/**
 * @brief A wrapper of parseBase58 function. It add the check if all characters in the input string are alphanumeric. If
 *not, it returns an empty optional, instead of calling the parseBase58 function.
 *
 *@tparam T The type of the value to parse to.
 *@param type The type of the token to parse.
 *@param str The string to parse.
 *@return An optional with the parsed value, or an empty optional if the parse fails.
 */
template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(ripple::TokenType type, std::string const& str)
{
    if (!std::all_of(std::begin(str), std::end(str), [](unsigned char c) { return std::isalnum(c); }))
        return std::nullopt;

    return ripple::parseBase58<T>(type, str);
}

}  // namespace util
