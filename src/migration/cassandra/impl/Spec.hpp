#pragma once

#include <boost/asio/spawn.hpp>

#include <concepts>
#include <tuple>
#include <type_traits>

namespace migration::cassandra::impl {
// Define the concept for a class like TableObjectsDesc
template <typename T>
concept TableSpec = requires {
    // Check that 'row' exists and is a tuple
    // keys types are at the beginning and the other fields types sort in alphabetical order
    typename T::Row;
    requires std::tuple_size_v<typename T::Row> >= 0;  // Ensures 'row' is a tuple

    // Check that static constexpr members 'partitionKey' and 'tableName' exist
    { T::kPARTITION_KEY } -> std::convertible_to<char const*>;
    { T::kTABLE_NAME } -> std::convertible_to<char const*>;
};
}  // namespace migration::cassandra::impl
