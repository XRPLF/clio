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

#include "rpc/common/APIVersion.hpp"
#include "util/newconfig/Error.hpp"
#include "util/newconfig/Types.hpp"

#include <fmt/core.h>
#include <fmt/format.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace util::config {
class ValueView;
class ConfigValue;

/**
 * @brief specific values that are accepted for logger levels in config.
 */
static constexpr std::array<char const*, 7> logLevels = {
    "trace",
    "debug",
    "info",
    "warning",
    "error",
    "fatal",
    "count",
};

/**
 * @brief specific values that are accepted for logger tag style in config.
 */
static constexpr std::array<char const*, 5> logTags = {
    "int",
    "uint",
    "null",
    "none",
    "uuid",
};

/**
 * @brief specific values that are accepted for cache loading in config.
 */
static constexpr std::array<char const*, 3> loadCacheMode = {
    "sync",
    "async",
    "none",
};

/**
 * @brief An interface to enforce constraints on certain values within ClioConfigDefinition.
 */
class Constraint {
public:
    // using "{}" instead of = default because of gcc bug. Bug is fixed in gcc13
    // see here for more info:
    // https://stackoverflow.com/questions/72835571/constexpr-c-error-destructor-used-before-its-definition
    constexpr virtual ~Constraint() noexcept {};

    /**
     * @brief Check if the value meets the specific constraint.
     *
     * @param val The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]]
    std::optional<Error>
    checkConstraint(Value const& val) const
    {
        if (auto const maybeError = checkTypeImpl(val); maybeError.has_value())
            return maybeError;
        return checkValueImpl(val);
    }

protected:
    /**
     * @brief Creates an error message for all constraints that must satisfy certain hard-coded values.
     *
     * @tparam arrSize, the size of the array of hardcoded values
     * @param key The key to the value
     * @param arr The array with hard-coded values to add to error message
     * @return The error message specifying what the value of key must be
     */
    template <std::size_t arrSize>
    constexpr std::string
    makeErrorMsg(std::string_view key, Value const& value, std::array<char const*, arrSize> arr) const
    {
        // Extract the value from the variant
        std::string valueStr = std::visit([](auto const& v) { return fmt::format("{}", v); }, value);

        // Create the error message
        auto errorMsg =
            fmt::format("You provided value \"{}\". Key \"{}\"'s value must be one of the following: ", valueStr, key);
        errorMsg += fmt::format("{}", fmt::join(arr, ", "));

        return errorMsg;
    }

    /**
     * @brief Check if the value is of a correct type for the constraint.
     *
     * @param val The value type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    virtual std::optional<Error>
    checkTypeImpl(Value const& val) const = 0;

    /**
     * @brief Check if the value is within the constraint.
     *
     * @param val The value type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    virtual std::optional<Error>
    checkValueImpl(Value const& val) const = 0;
};

/**
 * @brief A constraint to ensure the port number is within a valid range.
 */
class PortConstraint final : public Constraint {
public:
    constexpr ~PortConstraint()
    {
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param port The type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& port) const override;

    /**
     * @brief Check if the value is within the constraint.
     *
     * @param port The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& port) const override;

    static constexpr uint32_t portMin = 1;
    static constexpr uint32_t portMax = 65535;
};

/**
 * @brief A constraint to ensure the channel name is valid.
 */
class ChannelNameConstraint final : public Constraint {
public:
    constexpr ~ChannelNameConstraint()
    {
    }

private:
    /**
     * @brief Check if value is of type string.
     *
     * @param channelName The type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwis
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& channelName) const override;

    /**
     * @brief Check if the value is within the constraint.
     *
     * @param channelName The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& channelName) const override;
};

/**
 * @brief A constraint to ensure the log level name is valid.
 */
class LogLevelNameConstraint final : public Constraint {
public:
    constexpr ~LogLevelNameConstraint()
    {
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param logLevel The type to be checked
     * @return An optional Error if the constraint is not met, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& logLevel) const override;

    /**
     * @brief Check if the value is within the constraint.
     *
     * @param logLevel The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& logLevel) const override;
};

/**
 * @brief A constraint to ensure the IP address is valid.
 */
class ValidIPConstraint final : public Constraint {
public:
    constexpr ~ValidIPConstraint()
    {
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param ip The type to be checked.
     * @return An optional Error if the constraint is not met, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& ip) const override;

    /**
     * @brief Check if the value is within the constraint.
     *
     * @param ip The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& ip) const override;
};

/**
 * @brief A constraint to ensure the Cassandra name is valid.
 */
class CassandraName final : public Constraint {
public:
    constexpr ~CassandraName()
    {
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param name The type to be checked
     * @return An optional Error if the constraint is not met, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& name) const override;

    /**
     * @brief Check if the value is within the constraint
     *
     * @param name The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& name) const override;
};

/**
 * @brief A constraint to ensure the load mode is valid.
 */
class LoadConstraint final : public Constraint {
public:
    constexpr ~LoadConstraint()
    {
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param loadMode The type to be checked
     * @return An optional Error if the constraint is not met, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& loadMode) const override;

    /**
     * @brief Check if the value is within the constraint
     *
     * @param loadMode The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& loadMode) const override;
};

/**
 * @brief A constraint to ensure the log tag style is valid.
 */
class LogTagStyle final : public Constraint {
public:
    constexpr ~LogTagStyle()
    {
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param tagName The type to be checked
     * @return An optional Error if the constraint is not met, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& tagName) const override;

    /**
     * @brief Check if the value is within the constraint
     *
     * @param tagName The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& tagName) const override;
};

/**
 * @brief A constraint class to ensure an integer value is between two numbers (inclusive)
 */
template <typename numType>
class NumberValueConstraint final : public Constraint {
public:
    constexpr NumberValueConstraint(numType min, numType max) : min_{min}, max_{max}
    {
    }

    constexpr ~NumberValueConstraint()
    {
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param num The type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& num) const override
    {
        if (!std::holds_alternative<int64_t>(num))
            return Error{"Number must be of type integer"};
        return std::nullopt;
    }

    /**
     * @brief Check if the number is positive.
     *
     * @param num The number to check
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& num) const override
    {
        auto const numValue = std::holds_alternative<double>(num) ? std::get<double>(num) : std::get<int64_t>(num);
        if (numValue >= min_ && numValue <= max_)
            return std::nullopt;
        return Error{fmt::format("Number must be between {} and {}", min_, max_)};
    }

    numType min_;
    numType max_;
};

/**
 * @brief A constraint to ensure a double number is positive
 */
class PositiveDouble final : public Constraint {
public:
    constexpr ~PositiveDouble()
    {
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param num The type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& num) const override;

    /**
     * @brief Check if the number is positive.
     *
     * @param num The number to check
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& num) const override;
};

static constexpr PortConstraint const validatePort{};
static constexpr ChannelNameConstraint const validateChannelName{};
static constexpr LogLevelNameConstraint const validateLogLevelName{};
static constexpr ValidIPConstraint const validateIP{};
static constexpr CassandraName const validateCassandraName{};
static constexpr LoadConstraint const validateLoadMode{};
static constexpr LogTagStyle const validateLogTag{};
static constexpr PositiveDouble const validatePositiveDouble{};
static constexpr NumberValueConstraint<uint16_t> const validateUint16{
    std::numeric_limits<uint16_t>::min(),
    std::numeric_limits<uint16_t>::max()
};
static constexpr NumberValueConstraint<uint32_t> const validateUint32{
    std::numeric_limits<uint32_t>::min(),
    std::numeric_limits<uint32_t>::max()
};
static constexpr NumberValueConstraint<uint64_t> const validateUint64{
    std::numeric_limits<uint64_t>::min(),
    std::numeric_limits<uint64_t>::max()
};
static constexpr NumberValueConstraint<uint32_t> const validateApiVersion{rpc::API_VERSION_MIN, rpc::API_VERSION_MAX};

}  // namespace util::config
