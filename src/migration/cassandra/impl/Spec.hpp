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

#include <boost/asio/spawn.hpp>

#include <concepts>
#include <tuple>
#include <type_traits>

namespace migration::cassandra::impl {
// Define the concept for a class like TableObjectsDesc
template <typename T>
concept TableSpec = requires {
    // Check that 'row' exists and is a tuple
    // keys types are at the begining and the other fields types sort in alphabetical order
    typename T::Row;
    requires std::tuple_size<typename T::Row>::value >= 0;  // Ensures 'row' is a tuple

    // Check that static constexpr members 'partitionKey' and 'tableName' exist
    { T::kPARTITION_KEY } -> std::convertible_to<char const*>;
    { T::kTABLE_NAME } -> std::convertible_to<char const*>;
};
}  // namespace migration::cassandra::impl
