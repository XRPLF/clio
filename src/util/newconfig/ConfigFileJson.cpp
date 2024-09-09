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

#include "util/newconfig/ConfigFileJson.hpp"

#include "util/Assert.hpp"
#include "util/newconfig/Error.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parse_options.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>

#include <cstddef>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace util::config {

ConfigFileJson::ConfigFileJson(boost::json::object jsonObj)
{
    flattenJson(jsonObj, "");
}

std::expected<ConfigFileJson, Error>
ConfigFileJson::make_ConfigFileJson(boost::filesystem::path configFilePath)
{
    try {
        std::ifstream const in(configFilePath.string(), std::ios::in | std::ios::binary);
        if (in) {
            std::stringstream contents;
            contents << in.rdbuf();
            auto opts = boost::json::parse_options{};
            opts.allow_comments = true;
            auto const tempObj = boost::json::parse(contents.str(), {}, opts).as_object();
            return ConfigFileJson{tempObj};
        }
        return std::unexpected<Error>({fmt::format("Could not open configuration file '{}'", configFilePath.string())});
    } catch (std::exception const& e) {
        return std::unexpected<Error>({fmt::format(
            "An error occurred while processing configuration file '{}': {}", configFilePath.string(), e.what()
        )});
    }
}

/**
 * @brief Extracts the value from a JSON object and converts it into the corresponding type.
 *
 * @param jsonValue The JSON value to extract.
 * @return A variant containing the same type corresponding to the extracted value.
 */
[[nodiscard]] static Value
extractJsonValue(boost::json::value const& jsonValue)
{
    Value variantValue;

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

Value
ConfigFileJson::getValue(std::string_view key) const
{
    auto const jsonValue = jsonObject_.at(key);
    auto const value = extractJsonValue(jsonValue);
    return value;
}

std::vector<Value>
ConfigFileJson::getArray(std::string_view key) const
{
    ASSERT(jsonObject_.at(key).is_array(), "Key {} has value that is not an array", key);

    std::vector<Value> configValues;
    auto const arr = jsonObject_.at(key).as_array();

    for (auto const& item : arr) {
        auto const value = extractJsonValue(item);
        configValues.emplace_back(value);
    }
    return configValues;
}

bool
ConfigFileJson::containsKey(std::string_view key) const
{
    return jsonObject_.contains(key);
}

void
ConfigFileJson::flattenJson(boost::json::object const& obj, std::string const& prefix)
{
    for (auto const& [key, value] : obj) {
        std::string fullKey = prefix.empty() ? std::string(key) : fmt::format("{}.{}", prefix, std::string(key));

        // In ClioConfigDefinition, value must be a primitive or array
        if (value.is_object()) {
            flattenJson(value.as_object(), fullKey);
        } else if (value.is_array()) {
            auto const& arr = value.as_array();
            for (std::size_t i = 0; i < arr.size(); ++i) {
                std::string arrayPrefix = fullKey + ".[]";
                if (arr[i].is_object()) {
                    flattenJson(arr[i].as_object(), arrayPrefix);
                } else {
                    jsonObject_[arrayPrefix] = arr;
                }
            }
        } else {
            // if "[]" is present in key, then value must be an array instead of primitive
            if (fullKey.contains(".[]") && !jsonObject_.contains(fullKey)) {
                boost::json::array newArray;
                newArray.emplace_back(value);
                jsonObject_[fullKey] = newArray;
            } else if (fullKey.contains(".[]") && jsonObject_.contains(fullKey)) {
                jsonObject_[fullKey].as_array().emplace_back(value);
            } else {
                jsonObject_[fullKey] = value;
            }
        }
    }
}

}  // namespace util::config
