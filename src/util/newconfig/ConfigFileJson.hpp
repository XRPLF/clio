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

#include "util/newconfig/ConfigFileInterface.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/json/object.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace util::config {

/** @brief Json representation of config */
class ConfigFileJson final : public ConfigFileInterface {
public:
    /**
     * @brief Construct a new ConfigJson object and stores the values from
     * user's config in the object
     */
    ConfigFileJson(std::string_view filePath)
    {
        parse(filePath);
    }

    void
    parse(boost::filesystem::path filePath) override;

    /**
     * @brief Retrieves a configuration value by its key.
     *
     * @param key The key of the configuration value to retrieve.
     * @return A variant containing the same type corresponding to the extracted value.
     */
    std::variant<int64_t, std::string, bool, double>
    getValue(std::string_view key) const override;

    /**
     * @brief Retrieves an array of configuration values by its key.
     *
     * @param key The key of the configuration array to retrieve.
     * @return A vector of variants holding the config values specified by user.
     */
    std::vector<std::variant<int64_t, std::string, bool, double>>
    getArray(std::string_view key) const override;

    /**
     * @brief Checks if the configuration contains a specific key.
     *
     * @param key The key to check for.
     * @return True if the key exists, false otherwise.
     */
    bool
    containsKey(std::string_view key) const override;

private:
    /**
     * @brief Extracts the value from a JSON object and converts it into the corresponding type.
     *
     * @param jsonValue The JSON value to extract.
     * @return A variant containing the same type corresponding to the extracted value.
     */
    static std::variant<int64_t, std::string, bool, double>
    extractJsonValue(boost::json::value const& jsonValue)
    {
        std::variant<int64_t, std::string, bool, double> variantValue;

        if (jsonValue.is_int64()) {
            variantValue = jsonValue.as_int64();
        } else if (jsonValue.is_string()) {
            variantValue = jsonValue.as_string().c_str();
        } else if (jsonValue.is_bool()) {
            variantValue = jsonValue.as_bool();
        } else if (jsonValue.is_double()) {
            variantValue = jsonValue.as_double();
        }
        return variantValue;
    }

    /**
     * @brief Recursive function to flatten a JSON object into the same structure as the Clio Config,
     * with the same naming convensions for keys.
     *
     * @param obj The JSON object to flatten.
     * @param prefix The prefix to use for the keys in the flattened object.
     */
    void
    flattenJson(boost::json::object const& obj, std::string const& prefix);

    boost::json::object jsonObject_;
};

}  // namespace util::config
