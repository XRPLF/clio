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

#include "util/newconfig/Errors.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/filesystem/path.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace util::config {

/**
 * @brief The interface for configuration files.
 *
 * This class defines the interface for handling configuration files,
 * which can be implemented for different formats such as JSON or YAML.
 */
class ConfigFileInterface {
public:
    virtual ~ConfigFileInterface() = default;
    /**
     * @brief Parses the provided path of user clio configuration data
     *
     * @param filePath The path to the Clio Config data
     */
    virtual void
    parse(boost::filesystem::path filePath) = 0;

    /**
     * @brief Retrieves the value of configValue.
     *
     * @param key The key of configuration.
     * @return the value assosiated with key.
     */
    virtual Value
    getValue(std::string_view key) const = 0;

    /**
     * @brief Retrieves an array of configuration values.
     *
     * @param key The key of the configuration array.
     * @return A vector of configuration values if found, otherwise std::nullopt.
     */
    virtual std::vector<Value>
    getArray(std::string_view key) const = 0;

    /**
     * @brief Checks if key exist in configuration file.
     *
     * @param key The key to search for.
     * @return true if key exists in configuration file, false otherwise.
     */
    virtual bool
    containsKey(std::string_view key) const = 0;
};

}  // namespace util::config
