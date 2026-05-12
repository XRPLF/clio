#pragma once

#include "util/Assert.hpp"
#include "util/config/Array.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigFileInterface.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Error.hpp"
#include "util/config/ObjectView.hpp"
#include "util/config/ValueView.hpp"

#include <algorithm>
#include <chrono>
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

/**
 * @brief All the config data will be stored and extracted from this class
 *
 * Represents all the possible config data
 */
class ClioConfigDefinition {
public:
    using KeyValuePair = std::pair<std::string_view, std::variant<ConfigValue, Array>>;

    /**
     * @brief Constructs a new ClioConfigDefinition
     *
     * Initializes the configuration with a predefined set of key-value pairs
     * If a key contains "[]", the corresponding value must be an Array
     *
     * @param pair A list of key-value pairs for the predefined set of clio configurations
     */
    ClioConfigDefinition(std::initializer_list<KeyValuePair> pair);

    /**
     * @brief Parses the configuration file
     *
     * Also checks that no extra configuration key/value pairs are present. Adds to list of Errors
     * if it does
     *
     * @param config The configuration file interface
     * @return An optional vector of Error objects stating all the failures if parsing fails
     */
    [[nodiscard]] std::optional<std::vector<Error>>
    parse(ConfigFileInterface const& config);

    /**
     * @brief Returns the ObjectView specified with the prefix
     *
     * @param prefix The key prefix for the ObjectView
     * @param idx Used if getting Object in an Array
     * @return ObjectView with the given prefix
     */
    [[nodiscard]] ObjectView
    getObject(std::string_view prefix, std::optional<std::size_t> idx = std::nullopt) const;

    /**
     * @brief Returns the specified ValueView object associated with the key
     *
     * @param fullKey The config key to search for
     * @return ValueView associated with the given key
     */
    [[nodiscard]] ValueView
    getValueView(std::string_view fullKey) const;

    /**
     * @brief Returns the specified value of given string if value exists
     *
     * @tparam T The type T to return
     * @param fullKey The config key to search for
     * @return Value of key of type T
     */
    template <typename T>
    [[nodiscard]] T
    get(std::string_view fullKey) const
    {
        ASSERT(map_.contains(fullKey), "key {} does not exist in config", fullKey);
        auto const val = map_.at(fullKey);
        if (std::holds_alternative<ConfigValue>(val)) {
            return ValueView{std::get<ConfigValue>(val)}.getValueImpl<T>();
        }
        std::unreachable();
    }

    /**
     * @brief Returns the specified ValueView object in an array with a given index
     *
     * @param fullKey The config key to search for
     * @param index The index of the config value inside the Array to get
     * @return ValueView associated with the given key
     */
    [[nodiscard]] ValueView
    getValueInArray(std::string_view fullKey, std::size_t index) const;

    /**
     * @brief Returns the specified Array object from ClioConfigDefinition
     *
     * @param prefix The prefix to search config keys from
     * @return ArrayView with all key-value pairs where key starts with "prefix"
     */
    [[nodiscard]] ArrayView
    getArray(std::string_view prefix) const;

    /**
     * @brief Checks if a key is present in the configuration map.
     *
     * @param key The key to search for in the configuration map.
     * @return True if the key is present, false otherwise.
     */
    [[nodiscard]] bool
    contains(std::string_view key) const;

    /**
     * @brief Checks if any key in config starts with "key"
     *
     * @param key The key to search for in the configuration map.
     * @return True if the any key in config starts with "key", false otherwise.
     */
    [[nodiscard]] bool
    hasItemsWithPrefix(std::string_view key) const;

    /**
     * @brief Returns the Array object associated with the specified key.
     *
     * @param key The key whose associated Array object is to be returned.
     * @return The Array object associated with the specified key.
     */
    [[nodiscard]] Array const&
    asArray(std::string_view key) const;

    /**
     * @brief Returns the size of an Array
     *
     * @param prefix The prefix whose associated Array object is to be returned.
     * @return The size of the array associated with the specified prefix.
     */
    [[nodiscard]] std::size_t
    arraySize(std::string_view prefix) const;

    /**
     * @brief Method to convert a float seconds value to milliseconds.
     *
     * @param value The value to convert
     * @return The value in milliseconds
     */
    static std::chrono::milliseconds
    toMilliseconds(float value);

    /**
     * @brief Returns the specified value of given string of type T if type and value exists
     *
     * @tparam T The type T to return
     * @param fullKey The config key to search for
     * @return The value of type T if it exists, std::nullopt otherwise.
     */
    template <typename T>
    [[nodiscard]] std::optional<T>
    maybeValue(std::string_view fullKey) const
    {
        return getValueView(fullKey).asOptional<T>();
    }

    /**
     * @brief Returns an iterator to the beginning of the configuration map.
     *
     * @return A constant iterator to the beginning of the map.
     */
    [[nodiscard]] auto
    begin() const
    {
        return map_.begin();
    }

    /**
     * @brief Returns an iterator to the end of the configuration map.
     *
     * @return A constant iterator to the end of the map.
     */
    [[nodiscard]] auto
    end() const
    {
        return map_.end();
    }

private:
    /**
     * @brief returns the iterator of key-value pair with key fullKey
     *
     * @param fullKey Key to search for
     * @return iterator of map
     */
    [[nodiscard]] auto
    getArrayIterator(std::string_view key) const
    {
        auto const fullKey = addBracketsForArrayKey(key);
        auto const it =
            std::ranges::find_if(map_, [&fullKey](auto pair) { return pair.first == fullKey; });

        ASSERT(it != map_.end(), "key {} does not exist in config", fullKey);
        ASSERT(std::holds_alternative<Array>(it->second), "Value of {} is not an array", fullKey);

        return it;
    }

    /**
     * @brief Used for all Array API's; check to make sure "[]" exists, if not, append to end
     *
     * @param key key to check for
     * @return the key with "[]" appended to the end
     */
    [[nodiscard]] static std::string
    addBracketsForArrayKey(std::string_view key)
    {
        std::string fullKey = std::string(key);
        if (!key.contains(".[]"))
            fullKey += ".[]";
        return fullKey;
    }

    std::unordered_map<std::string_view, std::variant<ConfigValue, Array>> map_;
};

/**
 * @brief Full Clio Configuration definition.
 *
 * Specifies which keys are valid in Clio Config and provides default values if user's do not
 * specify one. Those without default values must be present in the user's config file.
 */
ClioConfigDefinition&
getClioConfig();

}  // namespace util::config
