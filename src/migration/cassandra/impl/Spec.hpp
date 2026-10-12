#pragma once

#include <boost/asio/spawn.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <tuple>

namespace migration::cassandra::impl {

// The number of columns in a comma-separated column list.
constexpr std::size_t
columnCount(std::string_view const selectColumns)
{
    return static_cast<std::size_t>(std::ranges::count(selectColumns, ',')) + 1;
}

// Define the concept for a class like TableObjectsDesc
template <typename T>
concept TableSpec = requires {
    // Check that 'Row' exists and is a tuple whose element types match kSelectColumns order
    typename T::Row;
    requires std::tuple_size_v<typename T::Row> >= 0;  // Ensures 'Row' is a tuple

    // Check that static constexpr members for the scan query exist.
    { T::kPartitionKey } -> std::convertible_to<char const*>;
    { T::kSelectColumns } -> std::convertible_to<char const*>;
    { T::kTableName } -> std::convertible_to<char const*>;

    // One selected column per Row element, so the two cannot drift apart when columns are added or
    // removed (a reorder of same-typed columns is still not detectable).
    requires columnCount(T::kSelectColumns) == std::tuple_size_v<typename T::Row>;
};
}  // namespace migration::cassandra::impl
