#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace util {

/**
 * @brief Specifies a number type
 */
template <typename T>
concept SomeNumberType = std::is_arithmetic_v<T> && !std::is_same_v<T, bool> && !std::is_const_v<T>;

/**
 * @brief Specifies a clock that reports the current time as a system clock time point
 */
template <typename T>
concept SomeSystemClock = requires {
    { T::now() } -> std::same_as<std::chrono::system_clock::time_point>;
};

/**
 * @brief Checks that the list of given values contains no duplicates
 *
 * @param values The list of values to check
 * @return true if no duplicates exist; false otherwise
 */
static consteval auto
hasNoDuplicates(auto&&... values)
{
    auto store = std::array{values...};
    auto end = store.end();
    std::ranges::sort(store);
    return (std::unique(std::begin(store), end) == end);
}

/**
 * @brief Checks that the list of given type contains no duplicates
 *
 * @tparam Types The types to check
 * @return true if no duplicates exist; false otherwise
 */
template <typename... Types>
constexpr bool
hasNoDuplicateNames()
{
    constexpr std::array<std::string_view, sizeof...(Types)> kNames = {Types::kName...};
    return !std::ranges::any_of(kNames, [&](std::string_view const& name1) {
        return std::ranges::any_of(kNames, [&](std::string_view const& name2) {
            return &name1 != &name2 && name1 == name2;  // Ensure different elements are compared
        });
    });
}

}  // namespace util
