//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "rpc/JS.hpp"

#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <algorithm>
#include <cctype>
#include <concepts>
#include <stdexcept>
#include <string>

/**
 * @brief This namespace contains various utilities.
 */
namespace util {

/**
 * @brief Convert a string to lowercase
 *
 * @param str The string to convert
 * @return The string in lowercase
 */
inline std::string
toLower(std::string str)
{
    std::ranges::transform(str, std::begin(str), [](unsigned char c) { return std::tolower(c); });
    return str;
}

/**
 * @brief Convert a string to uppercase
 *
 * @param str The string to convert
 * @return The string in uppercase
 */
inline std::string
toUpper(std::string str)
{
    std::ranges::transform(str, std::begin(str), [](unsigned char c) { return std::toupper(c); });
    return str;
}

/**
 * @brief Removes any detected secret information from a response JSON object.
 *
 * @param object The JSON object to remove secrets from
 * @return A secret-free copy of the passed object
 */
inline boost::json::object
removeSecret(boost::json::object const& object)
{
    auto newObject = object;
    auto const secretFields = {"secret", "seed", "seed_hex", "passphrase"};

    if (newObject.contains("params") and newObject.at("params").is_array() and
        not newObject.at("params").as_array().empty() and newObject.at("params").as_array()[0].is_object()) {
        for (auto const& secretField : secretFields) {
            if (newObject.at("params").as_array()[0].as_object().contains(secretField))
                newObject.at("params").as_array()[0].as_object()[secretField] = "*";
        }
    }

    // for websocket requests
    for (auto const& secretField : secretFields) {
        if (newObject.contains(secretField))
            newObject[secretField] = "*";
    }

    return newObject;
}

/**
 * @brief Detects the type of number stored in value and casts it back to the requested Type.
 * @note This conversion can possibly cause wrapping around or UB. Use with caution.
 *
 * @tparam Type The type to cast to
 * @param value The JSON value to cast
 * @return Value casted to the requested type
 * @throws logic_error if the underlying number is neither int64 nor uint64
 */
template <std::integral Type>
Type
integralValueAs(boost::json::value const& value)
{
    if (value.is_uint64())
        return static_cast<Type>(value.as_uint64());

    if (value.is_int64())
        return static_cast<Type>(value.as_int64());

    throw std::logic_error("Value neither uint64 nor int64");
}

/**
 * @brief Extracts ledger index from a JSON value which can be either a number or a string.
 *
 * @param value The JSON value to extract ledger index from
 * @return An optional containing the ledger index if it is a number; std::nullopt otherwise
 * @throws logic_error comes from integralValueAs if the underlying number is neither int64 nor uint64
 * @throws std::invalid_argument or std::out_of_range if the string cannot be converted to a number
 */
[[nodiscard]] inline std::optional<uint32_t>
getLedgerIndex(boost::json::value const& value)
{
    std::optional<uint32_t> ledgerIndex;

    if (not value.is_string()) {
        ledgerIndex = util::integralValueAs<uint32_t>(value);
    } else if (value.as_string() != "validated") {
        ledgerIndex = std::stoi(value.as_string().c_str());
    }

    return ledgerIndex;
}

}  // namespace util
