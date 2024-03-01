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

#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <algorithm>
#include <cctype>
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
    std::transform(std::begin(str), std::end(str), std::begin(str), [](unsigned char c) { return std::tolower(c); });
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
    std::transform(std::begin(str), std::end(str), std::begin(str), [](unsigned char c) { return std::toupper(c); });
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

}  // namespace util
