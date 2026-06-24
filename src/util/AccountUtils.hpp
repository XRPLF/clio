#pragma once

#include <xrpl/basics/StringUtilities.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/tokens.h>

#include <cctype>
#include <optional>
#include <string>

namespace util {

/**
 * @brief A wrapper of parseBase58 function. It adds the check if all characters in the input string
 * are alphanumeric. If not, it returns an empty optional, instead of calling the parseBase58
 * function.
 *
 * @tparam T The type of the value to parse to.
 * @param str The string to parse.
 * @return An optional with the parsed value, or an empty optional if the parse fails.
 */
template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(std::string const& str)
{
    if (!std::all_of(std::begin(str), std::end(str), [](unsigned char c) {
            return std::isalnum(c);
        }))
        return std::nullopt;

    return xrpl::parseBase58<T>(str);
}

/**
 * @brief A wrapper of parseBase58 function. It add the check if all characters in the input string
 * are alphanumeric. If not, it returns an empty optional, instead of calling the parseBase58
 * function.
 *
 * @tparam T The type of the value to parse to.
 * @param type The type of the token to parse.
 * @param str The string to parse.
 * @return An optional with the parsed value, or an empty optional if the parse fails.
 */
template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(xrpl::TokenType type, std::string const& str)
{
    if (!std::all_of(std::begin(str), std::end(str), [](unsigned char c) {
            return std::isalnum(c);
        }))
        return std::nullopt;

    return xrpl::parseBase58<T>(type, str);
}

}  // namespace util
