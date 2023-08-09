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

#include <util/config/detail/Helpers.h>

#include <boost/json.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace clio::util {

/**
 * @brief Convenience wrapper to query a JSON configuration file.
 *
 * Any custom data type can be supported by implementing the right `tag_invoke`
 * for `boost::json::value_to`.
 */
class Config final
{
    boost::json::value store_;
    static constexpr char Separator = '.';

public:
    using key_type = std::string;           /*! The type of key used */
    using array_type = std::vector<Config>; /*! The type of array used */
    using write_cursor_type = std::pair<std::optional<std::reference_wrapper<boost::json::value>>, key_type>;

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
    contains(key_type key) const;

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
    maybeValue(key_type key) const
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
    value(key_type key) const
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
    valueOr(key_type key, Result fallback) const
    {
        try
        {
            return maybeValue<Result>(key).value_or(fallback);
        }
        catch (detail::StoreException const&)
        {
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
    valueOrThrow(key_type key, std::string_view err) const
    {
        try
        {
            return maybeValue<Result>(key).value();
        }
        catch (std::exception const&)
        {
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
     * @return std::optional<array_type> Optional array
     * @throws std::logic_error Thrown if the key is of invalid format
     */
    [[nodiscard]] std::optional<array_type>
    maybeArray(key_type key) const;

    /**
     * @brief Interface for fetching an array by key.
     *
     * Will attempt to fetch an array under the desired key. If the array
     * exists then it will be
     * returned. If the array does not exist under the
     * specified key an std::logic_error is thrown.
     *
     * @param key The key to check
     * @return array_type The array
     * @throws std::logic_error Thrown if there is no array under the desired
     * key or the key is of invalid format
     */
    [[nodiscard]] array_type
    array(key_type key) const;

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
     * @return array_type The array
     * @throws std::logic_error Thrown if the key is of invalid format
     */
    [[nodiscard]] array_type
    arrayOr(key_type key, array_type fallback) const;

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
     * @return array_type The array
     * @throws std::runtime_error Thrown if there is no array under the desired
     * key
     */
    [[nodiscard]] array_type
    arrayOrThrow(key_type key, std::string_view err) const;

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
    section(key_type key) const;

    /**
     * @brief Interface for fetching a sub section by key with a fallback object.
     *
     * Will attempt to fetch an entire section under the desired key and return
     * it as a Config instance. If the section does not exist or another type is
     * stored under the desired key - fallback object is used instead.
     *
     * @param key The key to check
     * @param fallabkc The fallback object
     * @return Config Section represented as a separate instance of Config
     */
    [[nodiscard]] Config
    sectionOr(key_type key, boost::json::object fallback) const;

    //
    // Direct self-value access
    //

    /**
     * @brief Interface for reading the value directly referred to by the
     * instance. Wraps as std::optional.
     *
     * See @ref maybeValue(key_type) const for how this works.
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
     * See @ref value(key_type) const for how this works.
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
     * See @ref valueOr(key_type, Result) const for how this works.
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
     * See @ref valueOrThrow(key_type, std::string_view) const for how this
     * works.
     */
    template <typename Result>
    [[nodiscard]] Result
    valueOrThrow(std::string_view err) const
    {
        try
        {
            return maybeValue<Result>().value();
        }
        catch (std::exception const&)
        {
            throw std::runtime_error(err.data());
        }
    }

    /**
     * @brief Interface for reading the array directly referred to by the
     * instance.
     *
     * See @ref array(key_type) const for how this works.
     */
    [[nodiscard]] array_type
    array() const;

private:
    template <typename Return>
    [[nodiscard]] Return
    checkedAs(key_type key, boost::json::value const& value) const
    {
        using boost::json::value_to;

        auto has_error = false;
        if constexpr (std::is_same_v<Return, bool>)
        {
            if (not value.is_bool())
                has_error = true;
        }
        else if constexpr (std::is_same_v<Return, std::string>)
        {
            if (not value.is_string())
                has_error = true;
        }
        else if constexpr (std::is_same_v<Return, double>)
        {
            if (not value.is_number())
                has_error = true;
        }
        else if constexpr (std::is_convertible_v<Return, uint64_t> || std::is_convertible_v<Return, int64_t>)
        {
            if (not value.is_int64() && not value.is_uint64())
                has_error = true;
        }

        if (has_error)
            throw std::runtime_error(
                "Type for key '" + key + "' is '" + std::string{to_string(value.kind())} + "' in JSON but requested '" +
                detail::typeName<Return>() + "'");

        return value_to<Return>(value);
    }

    std::optional<boost::json::value>
    lookup(key_type key) const;

    write_cursor_type
    lookupForWrite(key_type key);
};

/**
 * @brief Simple configuration file reader.
 *
 * Reads the JSON file under specified path and creates a @ref Config object
 * from its contents.
 */
class ConfigReader final
{
public:
    static Config
    open(std::filesystem::path path);
};

}  // namespace clio::util
