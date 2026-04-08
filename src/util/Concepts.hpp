#pragma once

#include <algorithm>
#include <array>
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
 * @brief Checks that the list of given values contains no duplicates
 *
 * @param values The list of values to check
 * @returns true if no duplicates exist; false otherwise
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
 * @returns true if no duplicates exist; false otherwise
 */
template <typename... Types>
constexpr bool
hasNoDuplicateNames()
{
    constexpr std::array<std::string_view, sizeof...(Types)> kNAMES = {Types::kNAME...};
    return !std::ranges::any_of(kNAMES, [&](std::string_view const& name1) {
        return std::ranges::any_of(kNAMES, [&](std::string_view const& name2) {
            return &name1 != &name2 && name1 == name2;  // Ensure different elements are compared
        });
    });
}

}  // namespace util
