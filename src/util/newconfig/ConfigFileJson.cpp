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

#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parse_options.hpp>
#include <ripple/protocol/jss.h>

#include <exception>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace util::config {

using json = boost::json::object;

void
ConfigFileJson::parse(std::string_view configFilePath)
{
    try {
        std::ifstream const in(configFilePath, std::ios::in | std::ios::binary);
        if (in) {
            std::stringstream contents;
            contents << in.rdbuf();
            auto opts = boost::json::parse_options{};
            opts.allow_comments = true;
            jsonObject_ = boost::json::parse(contents.str(), {}, opts).as_object();
        }
    } catch (std::exception const& e) {
        LOG(util::LogService::error()) << "Could not read configuration file from '" << configFilePath
                                       << "': " << e.what();
    }
}

static ConfigValue
convertJsonToConfigValue(boost::json::value const& jsonValue)
{
    ConfigType jsonValueType;

    // Convert jsonValue to a type compatible with ConfigValue
    std::variant<int, char const*, bool, double> variantValue;

    if (jsonValue.is_int64()) {
        variantValue = static_cast<int>(jsonValue.as_int64());
        jsonValueType = getType<int>();
    } else if (jsonValue.is_string()) {
        variantValue = jsonValue.as_string().c_str();
        jsonValueType = getType<char const*>();
    } else if (jsonValue.is_bool()) {
        variantValue = jsonValue.as_bool();
        jsonValueType = getType<bool>();
    } else if (jsonValue.is_double()) {
        variantValue = jsonValue.as_double();
        jsonValueType = getType<double>();
    } else {
        throw std::runtime_error("Unsupported JSON value type");
    }

    return ConfigValue(jsonValueType).defaultValue(variantValue);
}

std::optional<ConfigValue>
ConfigFileJson::getValue(std::string_view key) const
{
    if (!jsonObject_.contains(key))
        return std::nullopt;

    auto jsonValue = jsonObject_.at(key);
    return convertJsonToConfigValue(jsonValue);
}

// recursive function in case in the future for config we have array inside array inside array...
// TODO: ask about have this function or just make getArray recursive and add params there.
// NOTE: TOOK A LONG TIME BUT FOUND BUG. BUT ASK WHY STRING_VIEW CORRUPTS THAT DATA.
void
goThroughJsonArray(
    boost::json::array const& arr,
    std::vector<ConfigFileJson::configVal>& configValues,
    std::string_view key = ""
)
{
    for (auto const& item : arr) {
        for (auto const& [itemKey, itemVal] : item.as_object()) {
            std::string embeddedKey = std::string(key) + "." + std::string(itemKey);

            if (itemVal.is_primitive()) {
                ConfigValue configValue = convertJsonToConfigValue(itemVal);
                configValues.emplace_back(embeddedKey, configValue);
            } else if (itemVal.is_array()) {
                goThroughJsonArray(itemVal.as_array(), configValues, embeddedKey);
            } else {
                throw std::runtime_error("json object not supported");
            }
        }
    }
}

// making this function recursive in the case of array in array in array etc..
std::optional<std::vector<ConfigFileJson::configVal>>
ConfigFileJson::getArray(std::string_view key) const
{
    if (!jsonObject_.contains(key))
        return std::nullopt;

    std::vector<ConfigFileJson::configVal> configValues;
    auto arr = jsonObject_.at(key).as_array();

    for (auto const& item : arr) {
        if (item.is_array())
            goThroughJsonArray(arr, configValues);

        for (auto const& [itemKey, itemVal] : item.as_object()) {
            std::string embeddedKey = std::string(itemKey);
            // array inside array
            if (itemVal.is_array()) {
                goThroughJsonArray(itemVal.as_array(), configValues, itemKey);
            } else if (itemVal.is_primitive()) {
                ConfigValue configValue = convertJsonToConfigValue(itemVal);
                configValues.emplace_back(std::string(itemKey), configValue);
            } else {
                throw std::runtime_error("json object not supported");
            }
        }
    }
    return configValues;
}

}  // namespace util::config
