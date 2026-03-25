#pragma once

#include "util/UnsupportedType.hpp"

#include <fmt/format.h>

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
