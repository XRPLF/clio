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

#include "util/UnsupportedType.hpp"

#include <fmt/core.h>

#include <cstdint>
#include <expected>
#include <ostream>
#include <string>
#include <variant>

namespace util::config {

/** @brief Custom clio config types */
enum class ConfigType { Integer, String, Double, Boolean };

/**
 * @brief Prints the specified config type to output stream
 *
 * @param stream The output stream
 * @param type The config type
 * @return The same ostream we were given
 */
std::ostream&
operator<<(std::ostream& stream, ConfigType type);

/** @brief Represents the supported Config Values */
using Value = std::variant<int64_t, std::string, bool, double>;

/**
 * @brief Prints the specified value to output stream
 *
 * @param stream The output stream
 * @param value The value type
 * @return The same ostream we were given
 */
std::ostream&
operator<<(std::ostream& stream, Value value);

/**
 * @brief Get the corresponding clio config type
 *
 * @tparam Type The type to get the corresponding ConfigType for
 * @return The corresponding ConfigType
 */
template <typename Type>
constexpr ConfigType
getType()
{
    if constexpr (std::is_same_v<Type, int64_t>) {
        return ConfigType::Integer;
    } else if constexpr (std::is_same_v<Type, std::string>) {
        return ConfigType::String;
    } else if constexpr (std::is_same_v<Type, double>) {
        return ConfigType::Double;
    } else if constexpr (std::is_same_v<Type, bool>) {
        return ConfigType::Boolean;
    } else {
        static_assert(util::Unsupported<Type>, "Wrong config type");
    }
}

}  // namespace util::config

