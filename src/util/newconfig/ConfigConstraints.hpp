
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

#include "util/newconfig/Errors.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>

namespace util::config {
class ValueView;
class ConfigValue;

/**
 * @brief specific values that are accepted for channel names in config
 */
static constexpr std::array<char const*, 7> channels = {
    "General",
    "WebServer",
    "Backend",
    "RPC",
    "ETL",
    "Subscriptions",
    "Performance",
};

/**
 * @brief specific values that are accepted for logger levels in config
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
 * @brief specific values that are accepted for logger tag style in config
 */
static constexpr std::array<char const*, 5> logTags = {
    "int",
    "uint",
    "null",
    "none",
    "uuid",
};

/**
 * @brief Minimum API version supported by this build
 *
 * Note: Clio does not natively support v1 and only supports v2 or newer.
 * However, Clio will forward all v1 requests to rippled for backward compatibility.
 */
static constexpr uint32_t API_VERSION_MIN = 1u;

/**
 * @brief Maximum API version supported by this build
 */
static constexpr uint32_t API_VERSION_MAX = 2u;

/**
 * @brief A interface to enforce constraints on certain values within ClioConfigDefinition.
 */
class Constraint {
public:
    using ValueType = std::variant<int64_t, std::string, bool, double>;
    virtual ~Constraint() noexcept = default;

    /**
     * @brief Check if the value meets the specific constraint.
     *
     * @param type The value to be checked.
     * @return An optional Error if the constraint is not met.
     */
    virtual std::optional<Error>
    checkConstraint(ValueType const& type) const = 0;

    /**
     * @brief Check if the value is of a correct type for the constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    virtual bool
    checkType(ValueType const& type) const = 0;
};

/**
 * @brief A constraint to ensure the port number is within a valid range.
 */
class PortConstraint final : public Constraint {
public:
    /**
     * @brief Check if the port number is within a valid range.
     *
     * @param port The port number to check.
     * @return An optional Error if the port is not within a valid range.
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(ValueType const& port) const override;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<int64_t>(type) || std::holds_alternative<std::string>(type);
    }

    uint32_t const portMin = 1;
    uint32_t const portMax = 65535;
};

/**
 * @brief A constraint to ensure the channel name is valid.
 */
class ChannelNameConstraint final : public Constraint {
public:
    /**
     * @brief Check if the channel name is valid.
     *
     * @param channel The channel name to check.
     * @return An optional Error if the channel name is not valid.
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(ValueType const& channel) const override;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<std::string>(type);
    }
};

/**
 * @brief A constraint to ensure the log level name is valid.
 */
class LogLevelNameConstraint final : public Constraint {
public:
    /**
     * @brief Check if the log level name is valid.
     *
     * @param logger The log level name to check.
     * @return An optional Error if the log level name is not valid.
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(ValueType const& logger) const override;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<std::string>(type);
    }
};

/**
 * @brief A constraint to ensure the IP address is valid.
 */
class ValidIPConstraint final : public Constraint {
public:
    /**
     * @brief Check if the IP address is valid.
     *
     * @param ip The IP address to check.
     * @return An optional Error if the IP address is not valid.
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(ValueType const& ip) const override;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<std::string>(type);
    }
};

/**
 * @brief A constraint to ensure the Cassandra name is valid.
 */
class CassandraName final : public Constraint {
public:
    /**
     * @brief Check if the Cassandra name is valid.
     *
     * @param name The Cassandra name to check.
     * @return An optional Error if the name is not "cassandra"
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(ValueType const& name) const override;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<std::string>(type);
    }
};

/**
 * @brief A constraint to ensure the load mode is valid.
 */
class LoadConstraint final : public Constraint {
public:
    /**
     * @brief Check if the load mode is valid.
     *
     * @param name The load mode to check.
     * @return An optional Error if the load mode is not valid.
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(ValueType const& name) const override;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<std::string>(type);
    }
};

/**
 * @brief A constraint to ensure the log tag style is valid.
 */
class LogTagStyle final : public Constraint {
public:
    /**
     * @brief Check if the log tag style is valid.
     *
     * @param tagName The log tag style to check.
     * @return An optional Error if the log tag style is not valid.
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(ValueType const& tagName) const override;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<std::string>(type);
    }
};

/**
 * @brief A constraint class to ensure the API version is within a valid range.
 */
class APIVersionConstraint final : public Constraint {
public:
    /**
     * @brief Check if the API version is within a valid range.
     *
     * @param apiVersion The API version to check.
     * @return An optional Error if the API version is not within a valid range.
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(ValueType const& apiVersion) const override;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The value type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<int64_t>(type);
    }
};

/**
 * @brief A constraint to ensure a number is positive and within the valid range of a given type.
 *
 * @tparam T The numeric type (ie., uint16_t, uint32_t etc).
 */
template <typename T>
class PositiveNumConstraint final : public Constraint {
public:
    /**
     * @brief Check if the number is positive and within the valid range.
     *
     * @param num The number to check.
     * @return An optional Error if the number is not valid.
     */
    [[nodiscard]] std::optional<Error>
    checkConstraint(Constraint::ValueType const& num) const override
    {
        if (!checkType(num))
            return Error{"number must be of type int"};
        auto const numToCheck = std::get<int64_t>(num);

        if (std::is_same_v<T, uint64_t> && numToCheck < 0)
            return Error{"uint64 must be positive"};

        if (std::is_same_v<T, uint64_t> || std::is_same_v<T, int64_t>)
            return std::nullopt;

        int64_t low = std::numeric_limits<T>::lowest();
        int64_t high = std::numeric_limits<T>::max();

        if (numToCheck >= low && numToCheck <= high)
            return std::nullopt;
        return Error{"num must be an integer"};
    }

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param type The type to be checked.
     * @return true if the type is correct, false otherwise.
     */
    [[nodiscard]] bool
    checkType(ValueType const& type) const override
    {
        return std::holds_alternative<std::int64_t>(type);
    }
};

static PortConstraint const validatePort{};
static ChannelNameConstraint const validateChannelName{};
static LogLevelNameConstraint const validateLogLevelName{};
static ValidIPConstraint const validateIP{};
static CassandraName const validateCassandraName{};
static LoadConstraint const validateLoadMode{};
static LogTagStyle const validateLogTag{};
static APIVersionConstraint const validateApiVersion{};
static PositiveNumConstraint<uint16_t> const ValidateUint16{};
static PositiveNumConstraint<uint32_t> const ValidateUint32{};
static PositiveNumConstraint<uint64_t> const ValidateUint64{};

}  // namespace util::config
