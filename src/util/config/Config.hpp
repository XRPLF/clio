//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/config/impl/Helpers.hpp"

#include <boost/json.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace util {

/**
 * @brief Convenience wrapper to query a JSON configuration file.
 *
 * Any custom data type can be supported by implementing the right `tag_invoke`
 * for `boost::json::value_to`.
 */
class Config final {
    boost::json::value store_;
    static constexpr char Separator = '.';

public:
    using KeyType = std::string;           /*< The type of key used */
    using ArrayType = std::vector<Config>; /*< The type of array used */
    using WriteCursorType = std::pair<std::optional<std::reference_wrapper<boost::json::value>>, KeyType>;

    /**
     * @brief Construct a new Config object.
     * @param store boost::json::value that backs this instance
     */
    explicit Config(boost::json::value store = {});

    //
    // Querying the store
    //

    /**
     * @brief Checks whether underlying store is not null.
     *
     * @return true If the store is null
     * @return false If the store is not null
     */
    operator bool() const noexcept;

    /**
     * @brief Checks whether something exists under given key.
     *
     * @param key The key to check
     * @return true If something exists under key
     * @return false If nothing exists under key
     * @throws std::logic_error If the key is of invalid format
     */
    [[nodiscard]] bool
    contains(KeyType key) const;

    //
    // Key value access
    //

    /**
     * @brief Interface for fetching values by key that returns std::optional.
     *
     * Will attempt to fetch the value under the desired key. If the value
     * exists and can be represented by the desired type Result then it will be
     * returned wrapped in an optional. If the value exists but the conversion
     * to Result is not possible - a runtime_error will be thrown. If the value
     * does not exist under the specified key - std::nullopt is returned.
     *
     * @tparam Result The desired return type
     * @param key The key to check
     * @return std::optional<Result> Optional value of desired type
     * @throws std::logic_error Thrown if conversion to Result is not possible
     * or key is of invalid format
     */
    template <typename Result>
    [[nodiscard]] std::optional<Result>
    maybeValue(KeyType key) const
    {
        auto maybe_element = lookup(key);
        if (maybe_element)
            return std::make_optional<Result>(checkedAs<Result>(key, *maybe_element));
        return std::nullopt;
    }

    /**
     * @brief Interface for fetching values by key.
     *
     * Will attempt to fetch the value under the desired key. If the value
     * exists and can be represented by the desired type Result then it will be
     * returned. If the value exists but the conversion
     * to Result is not possible OR the value does not exist - a logic_error
     * will be thrown.
     *
     * @tparam Result The desired return type
     * @param key The key to check
     * @return Result Value of desired type
     * @throws std::logic_error Thrown if conversion to Result is not
     * possible, value does not exist under specified key path or the key is of
     * invalid format
     */
    template <typename Result>
    [[nodiscard]] Result
    value(KeyType key) const
    {
        return maybeValue<Result>(key).value();
    }

    /**
     * @brief Interface for fetching values by key with fallback.
     *
     * Will attempt to fetch the value under the desired key. If the value
     * exists and can be represented by the desired type Result then it will be
     * returned. If the value exists but the conversion
     * to Result is not possible - a logic_error will be thrown. If the value
     * does not exist under the specified key - user specified fallback is
     * returned.
     *
     * @tparam Result The desired return type
     * @param key The key to check
     * @param fallback The fallback value
     * @return Result Value of desired type
     * @throws std::logic_error Thrown if conversion to Result is not possible
     * or the key is of invalid format
     */
    template <typename Result>
    [[nodiscard]] Result
    valueOr(KeyType key, Result fallback) const
    {
        try {
            return maybeValue<Result>(key).value_or(fallback);
        } catch (impl::StoreException const&) {
            return fallback;
        }
    }

    /**
     * @brief Interface for fetching values by key with custom error handling.
     *
     * Will attempt to fetch the value under the desired key. If the value
     * exists and can be represented by the desired type Result then it will be
     * returned. If the value exists but the conversion
     * to Result is not possible OR the value does not exist - a runtime_error
     * will be thrown with the user specified message.
     *
     * @tparam Result The desired return type
     * @param key The key to check
     * @param err The custom error message
     * @return Result Value of desired type
     * @throws std::runtime_error Thrown if conversion to Result is not possible
     * or value does not exist under key
     */
    template <typename Result>
    [[nodiscard]] Result
    valueOrThrow(KeyType key, std::string_view err) const
    {
        try {
            return maybeValue<Result>(key).value();
        } catch (std::exception const&) {
            throw std::runtime_error(err.data());
        }
    }

    /**
     * @brief Interface for fetching an array by key that returns std::optional.
     *
     * Will attempt to fetch an array under the desired key. If the array
     * exists then it will be
     * returned wrapped in an optional. If the array does not exist under the
     * specified key - std::nullopt is returned.
     *
     * @param key The key to check
     * @return std::optional<ArrayType> Optional array
     * @throws std::logic_error Thrown if the key is of invalid format
     */
    [[nodiscard]] std::optional<ArrayType>
    maybeArray(KeyType key) const;

    /**
     * @brief Interface for fetching an array by key.
     *
     * Will attempt to fetch an array under the desired key. If the array
     * exists then it will be
     * returned. If the array does not exist under the
     * specified key an std::logic_error is thrown.
     *
     * @param key The key to check
     * @return ArrayType The array
     * @throws std::logic_error Thrown if there is no array under the desired
     * key or the key is of invalid format
     */
    [[nodiscard]] ArrayType
    array(KeyType key) const;

    /**
     * @brief Interface for fetching an array by key with fallback.
     *
     * Will attempt to fetch an array under the desired key. If the array
     * exists then it will be returned.
     * If the array does not exist or another type is stored under the desired
     * key - user specified fallback is returned.
     *
     * @param key The key to check
     * @param fallback The fallback array
     * @return ArrayType The array
     * @throws std::logic_error Thrown if the key is of invalid format
     */
    [[nodiscard]] ArrayType
    arrayOr(KeyType key, ArrayType fallback) const;

    /**
     * @brief Interface for fetching an array by key with custom error handling.
     *
     * Will attempt to fetch an array under the desired key. If the array
     * exists then it will be returned.
     * If the array does not exist or another type is stored under the desired
     * key - std::runtime_error is thrown with the user specified error message.
     *
     * @param key The key to check
     * @param err The custom error message
     * @return ArrayType The array
     * @throws std::runtime_error Thrown if there is no array under the desired
     * key
     */
    [[nodiscard]] ArrayType
    arrayOrThrow(KeyType key, std::string_view err) const;

    /**
     * @brief Interface for fetching a sub section by key.
     *
     * Will attempt to fetch an entire section under the desired key and return
     * it as a Config instance. If the section does not exist or another type is
     * stored under the desired key - std::logic_error is thrown.
     *
     * @param key The key to check
     * @return Config Section represented as a separate instance of Config
     * @throws std::logic_error Thrown if there is no section under the
     * desired key or the key is of invalid format
     */
    [[nodiscard]] Config
    section(KeyType key) const;

    /**
     * @brief Interface for fetching a sub section by key with a fallback object.
     *
     * Will attempt to fetch an entire section under the desired key and return
     * it as a Config instance. If the section does not exist or another type is
     * stored under the desired key - fallback object is used instead.
     *
     * @param key The key to check
     * @param fallback The fallback object
     * @return Config Section represented as a separate instance of Config
     */
    [[nodiscard]] Config
    sectionOr(KeyType key, boost::json::object fallback) const;

    //
    // Direct self-value access
    //

    /**
     * @brief Interface for reading the value directly referred to by the
     * instance. Wraps as std::optional.
     *
     * See @ref maybeValue(KeyType) const for how this works.
     */
    template <typename Result>
    [[nodiscard]] std::optional<Result>
    maybeValue() const
    {
        if (store_.is_null())
            return std::nullopt;
        return std::make_optional<Result>(checkedAs<Result>("_self_", store_));
    }

    /**
     * @brief Interface for reading the value directly referred to by the
     * instance.
     *
     * See @ref value(KeyType) const for how this works.
     */
    template <typename Result>
    [[nodiscard]] Result
    value() const
    {
        return maybeValue<Result>().value();
    }

    /**
     * @brief Interface for reading the value directly referred to by the
     * instance with user-specified fallback.
     *
     * See @ref valueOr(KeyType, Result) const for how this works.
     */
    template <typename Result>
    [[nodiscard]] Result
    valueOr(Result fallback) const
    {
        return maybeValue<Result>().valueOr(fallback);
    }

    /**
     * @brief Interface for reading the value directly referred to by the
     * instance with user-specified error message.
     *
     * See @ref valueOrThrow(KeyType, std::string_view) const for how this
     * works.
     */
    template <typename Result>
    [[nodiscard]] Result
    valueOrThrow(std::string_view err) const
    {
        try {
            return maybeValue<Result>().value();
        } catch (std::exception const&) {
            throw std::runtime_error(err.data());
        }
    }

    /**
     * @brief Interface for reading the array directly referred to by the
     * instance.
     *
     * See @ref array(KeyType) const for how this works.
     */
    [[nodiscard]] ArrayType
    array() const;

private:
    template <typename Return>
    [[nodiscard]] Return
    checkedAs(KeyType key, boost::json::value const& value) const
    {
        using boost::json::value_to;

        auto has_error = false;
        if constexpr (std::is_same_v<Return, bool>) {
            if (not value.is_bool())
                has_error = true;
        } else if constexpr (std::is_same_v<Return, std::string>) {
            if (not value.is_string())
                has_error = true;
        } else if constexpr (std::is_same_v<Return, double>) {
            if (not value.is_number())
                has_error = true;
        } else if constexpr (std::is_convertible_v<Return, uint64_t> || std::is_convertible_v<Return, int64_t>) {
            if (not value.is_int64() && not value.is_uint64())
                has_error = true;
        }

        if (has_error) {
            throw std::runtime_error(
                "Type for key '" + key + "' is '" + std::string{to_string(value.kind())} + "' in JSON but requested '" +
                impl::typeName<Return>() + "'"
            );
        }

        return value_to<Return>(value);
    }

    std::optional<boost::json::value>
    lookup(KeyType key) const;

    WriteCursorType
    lookupForWrite(KeyType key);
};

/**
 * @brief Simple configuration file reader.
 *
 * Reads the JSON file under specified path and creates a @ref Config object
 * from its contents.
 */
class ConfigReader final {
public:
    static Config
    open(std::filesystem::path path);
};

}  // namespace util
