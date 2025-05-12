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

#include "util/config/ConfigDefinition.hpp"

#include "util/Assert.hpp"
#include "util/Constants.hpp"
#include "util/OverloadSet.hpp"
#include "util/config/Array.hpp"
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigFileInterface.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Error.hpp"
#include "util/config/ObjectView.hpp"
#include "util/config/Types.hpp"
#include "util/config/ValueView.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace util::config {

ClioConfigDefinition::ClioConfigDefinition(std::initializer_list<KeyValuePair> pair)
{
    for (auto const& [key, value] : pair) {
        if (key.contains("[]"))
            ASSERT(std::holds_alternative<Array>(value), R"(Value must be array if key has "[]")");
        map_.insert({key, value});
    }
}

ObjectView
ClioConfigDefinition::getObject(std::string_view prefix, std::optional<std::size_t> idx) const
{
    auto const prefixWithDot = std::string(prefix) + ".";
    for (auto const& [mapKey, mapVal] : map_) {
        auto const hasPrefix = mapKey.starts_with(prefixWithDot);
        if (idx.has_value() && hasPrefix && std::holds_alternative<Array>(mapVal)) {
            ASSERT(std::get<Array>(mapVal).size() > idx.value(), "Index provided is out of scope");

            // we want to support getObject("array") and getObject("array.[]"), so we check if "[]" exists
            if (!prefix.contains("[]"))
                return ObjectView{prefixWithDot + "[]", idx.value(), *this};
            return ObjectView{prefix, idx.value(), *this};
        }
        if (hasPrefix && !idx.has_value() && !mapKey.contains(prefixWithDot + "[]"))
            return ObjectView{prefix, *this};
    }
    ASSERT(false, "Key {} is not found in config", prefix);
    std::unreachable();
}

ArrayView
ClioConfigDefinition::getArray(std::string_view prefix) const
{
    auto const key = addBracketsForArrayKey(prefix);

    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(key)) {
            ASSERT(
                std::holds_alternative<Array>(mapVal), "Trying to retrieve Object or ConfigValue, instead of an Array "
            );
            return ArrayView{key, *this};
        }
    }
    ASSERT(false, "Key {} is not found in config", key);
    std::unreachable();
}

bool
ClioConfigDefinition::contains(std::string_view key) const
{
    return map_.contains(key);
}

bool
ClioConfigDefinition::hasItemsWithPrefix(std::string_view key) const
{
    return std::ranges::any_of(map_, [&key](auto const& pair) { return pair.first.starts_with(key); });
}

ValueView
ClioConfigDefinition::getValueView(std::string_view fullKey) const
{
    ASSERT(map_.contains(fullKey), "key {} does not exist in config", fullKey);
    if (std::holds_alternative<ConfigValue>(map_.at(fullKey))) {
        return ValueView{std::get<ConfigValue>(map_.at(fullKey))};
    }
    ASSERT(false, "Value of key {} is an Array, not an object", fullKey);
    std::unreachable();
}

std::chrono::milliseconds
ClioConfigDefinition::toMilliseconds(float value)
{
    ASSERT(value >= 0.0f, "Floating point value of seconds must be non-negative, got: {}", value);
    return std::chrono::milliseconds{std::lroundf(value * static_cast<float>(util::kMILLISECONDS_PER_SECOND))};
}

ValueView
ClioConfigDefinition::getValueInArray(std::string_view fullKey, std::size_t index) const
{
    auto const it = getArrayIterator(fullKey);
    return ValueView{std::get<Array>(it->second).at(index)};
}

Array const&
ClioConfigDefinition::asArray(std::string_view fullKey) const
{
    auto const it = getArrayIterator(fullKey);
    return std::get<Array>(it->second);
}

std::size_t
ClioConfigDefinition::arraySize(std::string_view prefix) const
{
    auto const key = addBracketsForArrayKey(prefix);

    for (auto const& pair : map_) {
        if (pair.first.starts_with(key)) {
            return std::get<Array>(pair.second).size();
        }
    }
    ASSERT(false, "Prefix {} not found in any of the config keys", key);
    std::unreachable();
}

std::optional<std::vector<Error>>
ClioConfigDefinition::parse(ConfigFileInterface const& config)
{
    std::vector<Error> listOfErrors;
    std::unordered_map<std::string_view, std::vector<std::string_view>> arrayPrefixesToKeysMap;
    for (auto& [key, value] : map_) {
        if (key.contains(".[]")) {
            auto const prefix = Array::prefix(key);
            arrayPrefixesToKeysMap[prefix].push_back(key);
        }

        // if key doesn't exist in user config, makes sure it is marked as ".optional()" or has ".defaultValue()"" in
        // ClioConfigDefinition above
        if (!config.containsKey(key)) {
            if (std::holds_alternative<ConfigValue>(value)) {
                if (!(std::get<ConfigValue>(value).isOptional() || std::get<ConfigValue>(value).hasValue()))
                    listOfErrors.emplace_back(key, "key is required in user Config");
            }
            continue;
        }
        ASSERT(
            std::holds_alternative<ConfigValue>(value) || std::holds_alternative<Array>(value),
            "Value must be of type ConfigValue or Array"
        );
        std::visit(
            util::OverloadSet{// handle the case where the config value is a single element.
                              // attempt to set the value from the configuration for the specified key.
                              [&key, &config, &listOfErrors](ConfigValue& val) {
                                  if (auto const maybeError = val.setValue(config.getValue(key), key);
                                      maybeError.has_value()) {
                                      listOfErrors.emplace_back(maybeError.value());
                                  }
                              },
                              // handle the case where the config value is an array.
                              // iterate over each provided value in the array and attempt to set it for the key.
                              [&key, &config, &listOfErrors](Array& arr) {
                                  for (auto const& val : config.getArray(key)) {
                                      if (val.has_value()) {
                                          if (auto const maybeError = arr.addValue(*val, key); maybeError.has_value()) {
                                              listOfErrors.emplace_back(*maybeError);
                                          }
                                      } else {
                                          if (auto const maybeError = arr.addNull(key); maybeError.has_value()) {
                                              listOfErrors.emplace_back(*maybeError);
                                          }
                                      }
                                  }
                              }
            },
            value
        );
    }

    if (!listOfErrors.empty())
        return listOfErrors;

    // The code above couldn't detect whether some fields in an array are missing.
    // So to fix it for each array we determine it's size and add empty values if the field is optional
    // or generate an error.
    for (auto const& [_, keys] : arrayPrefixesToKeysMap) {
        size_t maxSize = 0;
        std::ranges::for_each(keys, [&](std::string_view key) {
            ASSERT(std::holds_alternative<Array>(map_.at(key)), "{} is not array", key);
            maxSize = std::max(maxSize, arraySize(key));
        });
        if (maxSize == 0) {
            // empty arrays are allowed
            continue;
        }

        std::ranges::for_each(keys, [&](std::string_view key) {
            auto& array = std::get<Array>(map_.at(key));
            while (array.size() < maxSize) {
                auto const err = array.addNull(key);
                if (err.has_value()) {
                    listOfErrors.emplace_back(*err);
                    break;
                }
            }
        });
    }

    if (!listOfErrors.empty())
        return listOfErrors;

    return std::nullopt;
}

}  // namespace util::config
