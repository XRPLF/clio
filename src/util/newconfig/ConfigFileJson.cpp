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
#include "util/log/Logger.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parse_options.hpp>
#include <boost/json/value.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace util::config {

void
ConfigFileJson::parse(boost::filesystem::path configFilePath)
{
    try {
        std::ifstream const in(configFilePath.string(), std::ios::in | std::ios::binary);
        if (in) {
            std::stringstream contents;
            contents << in.rdbuf();
            auto opts = boost::json::parse_options{};
            opts.allow_comments = true;
            auto const tempObj = boost::json::parse(contents.str(), {}, opts).as_object();
            flattenJson(tempObj, "");
        }
    } catch (std::exception const& e) {
        LOG(util::LogService::error()) << "Could not read configuration file from '" << configFilePath.string()
                                       << "': " << e.what();
    }
}

std::variant<int64_t, std::string, bool, double>
ConfigFileJson::getValue(std::string_view key) const
{
    ASSERT(jsonObject_.contains(key), "Json object does not contain key {}", key);
    auto const jsonValue = jsonObject_.at(key);

    auto const value = extractJsonValue(jsonValue);
    return value;
}

std::vector<std::variant<int64_t, std::string, bool, double>>
ConfigFileJson::getArray(std::string_view key) const
{
    ASSERT(jsonObject_.contains(key), "Key {} must exist in Json", key);
    ASSERT(jsonObject_.at(key).is_array(), "Key {} has value that is not an array", key);

    std::vector<std::variant<int64_t, std::string, bool, double>> configValues;
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
        std::string fullKey = prefix.empty() ? std::string(key) : prefix + "." + std::string(key);

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
