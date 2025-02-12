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
#include "util/newconfig/Array.hpp"
#include "util/newconfig/Error.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parse_options.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace util::config {

namespace {
/**
 * @brief Extracts the value from a JSON object and converts it into the corresponding type.
 *
 * @param jsonValue The JSON value to extract.
 * @return A variant containing the same type corresponding to the extracted value.
 */
[[nodiscard]] Value
extractJsonValue(boost::json::value const& jsonValue)
{
    if (jsonValue.is_int64()) {
        return jsonValue.as_int64();
    }
    if (jsonValue.is_uint64()) {
        return static_cast<int64_t>(jsonValue.as_uint64());
    }
    if (jsonValue.is_string()) {
        return jsonValue.as_string().c_str();
    }
    if (jsonValue.is_bool()) {
        return jsonValue.as_bool();
    }
    if (jsonValue.is_double()) {
        return jsonValue.as_double();
    }
    if (jsonValue.is_null()) {
        return NullType{};
    }
    ASSERT(false, "Json is not of type null, int, uint, string, bool or double");
    std::unreachable();
}
}  // namespace

ConfigFileJson::ConfigFileJson(boost::json::object jsonObj)
{
    flattenJson(jsonObj);
}

std::expected<ConfigFileJson, Error>
ConfigFileJson::makeConfigFileJson(std::filesystem::path const& configFilePath)
{
    try {
        if (auto const in = std::ifstream(configFilePath.string(), std::ios::in | std::ios::binary); in) {
            std::stringstream contents;
            contents << in.rdbuf();
            auto const opts = boost::json::parse_options{.allow_comments = true};
            auto const tempObj = boost::json::parse(contents.str(), {}, opts).as_object();
            return ConfigFileJson{tempObj};
        }
        return std::unexpected<Error>(
            Error{fmt::format("Could not open configuration file '{}'", configFilePath.string())}
        );

    } catch (std::exception const& e) {
        return std::unexpected<Error>(Error{fmt::format(
            "An error occurred while processing configuration file '{}': {}", configFilePath.string(), e.what()
        )});
    }
}

Value
ConfigFileJson::getValue(std::string_view key) const
{
    ASSERT(containsKey(key), "Key {} not found in ConfigFileJson", key);
    auto const jsonValue = jsonObject_.at(key);
    ASSERT(jsonValue.is_primitive(), "Key {} has value that is not a primitive", key);
    auto const value = extractJsonValue(jsonValue);
    return value;
}

std::vector<Value>
ConfigFileJson::getArray(std::string_view key) const
{
    ASSERT(containsKey(key), "Key {} not found in ConfigFileJson", key);
    ASSERT(jsonObject_.at(key).is_array(), "Key {} has value that is not an array", key);

    std::vector<Value> configValues;
    auto const arr = jsonObject_.at(key).as_array();

    for (auto const& item : arr) {
        auto value = extractJsonValue(item);
        configValues.emplace_back(std::move(value));
    }
    return configValues;
}

bool
ConfigFileJson::containsKey(std::string_view key) const
{
    return jsonObject_.contains(key);
}

boost::json::object const&
ConfigFileJson::inner() const
{
    return jsonObject_;
}

void
ConfigFileJson::flattenJson(boost::json::object const& jsonRootObject)
{
    struct Task {
        boost::json::object const& object;
        std::string prefix;
        std::optional<size_t> arrayIndex = std::nullopt;
    };

    std::queue<Task> tasks;
    tasks.push(Task{.object = jsonRootObject, .prefix = ""});

    std::unordered_map<std::string, size_t> arraysSizes;

    while (not tasks.empty()) {
        auto const task = std::move(tasks.front());
        tasks.pop();

        for (auto const& [key, value] : task.object) {
            auto fullKey =
                task.prefix.empty() ? std::string(key) : fmt::format("{}.{}", task.prefix, std::string_view{key});

            if (value.is_object()) {
                tasks.push(
                    Task{.object = value.as_object(), .prefix = std::move(fullKey), .arrayIndex = task.arrayIndex}
                );
            } else if (value.is_array()) {
                fullKey += ".[]";
                auto const& array = value.as_array();

                if (std::ranges::all_of(array, [](auto const& v) { return v.is_primitive(); })) {
                    jsonObject_[fullKey] = array;
                } else if (std::ranges::all_of(array, [](auto const& v) { return v.is_object(); })) {
                    for (size_t i = 0; i < array.size(); ++i) {
                        tasks.push(Task{.object = array.at(i).as_object(), .prefix = fullKey, .arrayIndex = i});
                    }
                } else {
                    ASSERT(
                        false,
                        "Arrays containing both values and objects are not supported. Please check the array {}",
                        fullKey
                    );
                }
            } else {
                if (task.arrayIndex.has_value()) {
                    if (not jsonObject_.contains(fullKey)) {
                        jsonObject_[fullKey] = boost::json::array{};
                    }

                    auto& targetArray = jsonObject_.at(fullKey).as_array();
                    while (targetArray.size() < (*task.arrayIndex + 1)) {
                        targetArray.push_back(boost::json::value());
                    }
                    targetArray.at(*task.arrayIndex) = value;
                    auto const prefix = std::string{Array::prefix(fullKey)};
                    arraysSizes[prefix] = std::max(arraysSizes[prefix], targetArray.size());
                } else {
                    jsonObject_[fullKey] = value;
                }
            }
        }
    }

    // adjust length of each array containing objects
    std::ranges::for_each(jsonObject_, [&arraysSizes](auto& item) {
        auto const key = item.key();
        if (not key.contains("[]"))
            return;

        auto& value = item.value();
        auto const prefix = std::string{Array::prefix(key)};
        if (auto const it = arraysSizes.find(prefix); it != arraysSizes.end()) {
            auto const size = it->second;
            while (value.as_array().size() < size) {
                value.as_array().push_back(boost::json::value{});
            }
        }
    });
}

}  // namespace util::config
