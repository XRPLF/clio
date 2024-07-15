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
<<<<<<< HEAD
<<<<<<< HEAD
#include "util/newconfig/ConfigDefinition.hpp"
=======
>>>>>>> d2f765f (Commit work so far)
#include "util/newconfig/ConfigValue.hpp"

#include <boost/json/array.hpp>
=======
#include "util/newconfig/ConfigValue.hpp"

>>>>>>> e62e648 (first draft of config)
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parse_options.hpp>
#include <ripple/protocol/jss.h>

#include <exception>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
<<<<<<< HEAD
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace util::config {

using json = boost::json::object;

=======
#include <string_view>

namespace util::config {

>>>>>>> e62e648 (first draft of config)
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
<<<<<<< HEAD
            jsonObject_ = boost::json::parse(contents.str(), {}, opts).as_object();
=======
            object_ = boost::json::parse(contents.str(), {}, opts).as_object();
>>>>>>> e62e648 (first draft of config)
        }
    } catch (std::exception const& e) {
        LOG(util::LogService::error()) << "Could not read configuration file from '" << configFilePath
                                       << "': " << e.what();
    }
}

<<<<<<< HEAD
static ConfigValue
convertJsonToConfigValue(boost::json::value const& jsonValue)
{
    ConfigType jsonValueType;

    // Convert jsonValue to a type compatible with ConfigValue
    std::variant<int, std::string, bool, double> variantValue;

    if (jsonValue.is_int64()) {
        variantValue = static_cast<int>(jsonValue.as_int64());
        jsonValueType = getType<int>();
    } else if (jsonValue.is_string()) {
        variantValue = jsonValue.as_string().c_str();
        jsonValueType = getType<std::string>();
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
goThroughJsonArray(boost::json::array const& arr, std::vector<ConfigValue>& configValues)
{
    for (auto const& item : arr) {
        for (auto const& [itemKey, itemVal] : item.as_object()) {
            if (itemVal.is_primitive()) {
                ConfigValue configValue = convertJsonToConfigValue(itemVal);
                configValues.emplace_back(configValue);
            } else if (itemVal.is_array()) {
                goThroughJsonArray(itemVal.as_array(), configValues);
            } else {
                throw std::runtime_error("json object not supported");
            }
        }
    }
}

// making this function recursive in the case of array in array in array etc..
std::optional<std::vector<ConfigValue>>
ConfigFileJson::getArray(std::string_view key) const
{
    if (!jsonObject_.contains(key))
        return std::nullopt;

    std::vector<ConfigValue> configValues;
    auto arr = jsonObject_.at(key).as_array();

    for (auto const& item : arr) {
        if (item.is_array())
            goThroughJsonArray(arr, configValues);

        for (auto const& [itemKey, itemVal] : item.as_object()) {
            // array inside array
            if (itemVal.is_array()) {
                goThroughJsonArray(itemVal.as_array(), configValues);
            } else if (itemVal.is_primitive()) {
                ConfigValue configValue = convertJsonToConfigValue(itemVal);
                configValues.emplace_back(configValue);
            } else {
                throw std::runtime_error("json object not supported");
            }
        }
    }
    return configValues;
=======
std::optional<ValueData<ConfigType>>
ConfigFileJson::getValue(std::string_view key) const
{
    if (!object_.contains(key))
        return std::nullopt;

    /*
    auto jsonValue = object_.at(key);
        if (jsonValue.is_int64()) {
            return ValueData<getType<int>()>{jsonValue.as_int64()};
        } else if (jsonValue.is_string()) {
            return ValueData<ConfigType::String>{jsonValue.as_string()};
        } else if (jsonValue.is_double()) {
            return ValueData<ConfigType::Float>{jsonValue.as_double()};
        } else if (jsonValue.is_bool()) {
            return ValueData<ConfigType::Boolean>{jsonValue.as_bool()};
        }

     */
    return std::nullopt;
>>>>>>> e62e648 (first draft of config)
}

}  // namespace util::config
