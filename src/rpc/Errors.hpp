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

/** @file */
#pragma once

#include <boost/json/object.hpp>
#include <ripple/protocol/ErrorCodes.h>

#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace rpc {

/** @brief Custom clio RPC Errors. */
enum class ClioError {
    // normal clio errors start with 5000
    rpcMALFORMED_CURRENCY = 5000,
    rpcMALFORMED_REQUEST = 5001,
    rpcMALFORMED_OWNER = 5002,
    rpcMALFORMED_ADDRESS = 5003,
    rpcINVALID_HOT_WALLET = 5004,
    rpcUNKNOWN_OPTION = 5005,
    rpcFIELD_NOT_FOUND_TRANSACTION = 5006,

    // special system errors start with 6000
    rpcINVALID_API_VERSION = 6000,
    rpcCOMMAND_IS_MISSING = 6001,
    rpcCOMMAND_NOT_STRING = 6002,
    rpcCOMMAND_IS_EMPTY = 6003,
    rpcPARAMS_UNPARSEABLE = 6004,
};

/** @brief Holds info about a particular @ref ClioError. */
struct ClioErrorInfo {
    ClioError const code;
    std::string_view const error;
    std::string_view const message;
};

/** @brief Clio uses compatible Rippled error codes for most RPC errors. */
using RippledError = ripple::error_code_i;

/**
 * @brief Clio operates on a combination of Rippled and Custom Clio error codes.
 *
 * @see RippledError For rippled error codes
 * @see ClioError For custom clio error codes
 */
using CombinedError = std::variant<RippledError, ClioError>;

/** @brief A status returned from any RPC handler. */
struct Status {
    CombinedError code = RippledError::rpcSUCCESS;
    std::string error;
    std::string message;
    std::optional<boost::json::object> extraInfo;

    Status() = default;

    /**
     * @brief Construct a new Status object
     *
     * @param code The error code
     */
    /* implicit */ Status(CombinedError code) : code(code){};

    /**
     * @brief Construct a new Status object
     *
     * @param code The error code
     * @param extraInfo The extra info
     */
    Status(CombinedError code, boost::json::object&& extraInfo) : code(code), extraInfo(std::move(extraInfo)){};

    /**
     * @brief Construct a new Status object with a custom message
     *
     * @note HACK. Some rippled handlers explicitly specify errors. This means that we have to be able to duplicate this
     * functionality.
     *
     * @param message The message
     */
    explicit Status(std::string message) : code(ripple::rpcUNKNOWN), message(std::move(message))
    {
    }

    /**
     * @brief Construct a new Status object
     *
     * @param code The error code
     * @param message The message
     */
    Status(CombinedError code, std::string message) : code(code), message(std::move(message))
    {
    }

    /**
     * @brief Construct a new Status object
     *
     * @param code The error code
     * @param error The error
     * @param message The message
     */
    Status(CombinedError code, std::string error, std::string message)
        : code(code), error(std::move(error)), message(std::move(message))
    {
    }

    /**
     * @brief Check if the status is not OK
     *
     * @return true if the status is not OK; false otherwise
     */
    operator bool() const
    {
        if (auto err = std::get_if<RippledError>(&code))
            return *err != RippledError::rpcSUCCESS;

        return true;
    }

    /**
     * @brief Returns true if the @ref rpc::Status contains the desired @ref rpc::RippledError
     *
     * @param other The @ref rpc::RippledError to match
     * @return true if status matches given error; false otherwise
     */
    bool
    operator==(RippledError other) const
    {
        if (auto err = std::get_if<RippledError>(&code))
            return *err == other;

        return false;
    }

    /**
     * @brief Returns true if the Status contains the desired @ref ClioError
     *
     * @param other The RippledError to match
     * @return true if status matches given error; false otherwise
     */
    bool
    operator==(ClioError other) const
    {
        if (auto err = std::get_if<ClioError>(&code))
            return *err == other;

        return false;
    }
};

/** @brief Warning codes that can be returned by clio. */
enum WarningCode {
    warnUNKNOWN = -1,
    warnRPC_CLIO = 2001,
    warnRPC_OUTDATED = 2002,
    warnRPC_RATE_LIMIT = 2003,
    warnRPC_DEPRECATED = 2004
};

/** @brief Holds information about a clio warning. */
struct WarningInfo {
    constexpr WarningInfo() = default;

    /**
     * @brief Construct a new Warning Info object
     *
     * @param code The warning code
     * @param message The warning message
     */
    constexpr WarningInfo(WarningCode code, char const* message) : code(code), message(message)
    {
    }

    WarningCode code = warnUNKNOWN;
    std::string_view const message = "unknown warning";
};

/** @brief Invalid parameters error. */
class InvalidParamsError : public std::exception {
    std::string msg;

public:
    /**
     * @brief Construct a new Invalid Params Error object
     *
     * @param msg The error message
     */
    explicit InvalidParamsError(std::string msg) : msg(std::move(msg))
    {
    }

    /**
     * @brief Get the error message as a C string
     *
     * @return The error message
     */
    char const*
    what() const throw() override
    {
        return msg.c_str();
    }
};

/** @brief Account not found error. */
class AccountNotFoundError : public std::exception {
    std::string account;

public:
    /**
     * @brief Construct a new Account Not Found Error object
     *
     * @param acct The account
     */
    explicit AccountNotFoundError(std::string acct) : account(std::move(acct))
    {
    }

    /**
     * @brief Get the error message as a C string
     *
     * @return The error message
     */
    char const*
    what() const throw() override
    {
        return account.c_str();
    }
};

/** @brief A globally available @ref rpc::Status that represents a successful state. */
static Status OK;

/**
 * @brief Get the warning info object from a warning code.
 *
 * @param code The warning code
 * @return A reference to the static warning info
 */
WarningInfo const&
getWarningInfo(WarningCode code);

/**
 * @brief Get the error info object from an clio-specific error code.
 *
 * @param code The error code
 * @return A reference to the static error info
 */
ClioErrorInfo const&
getErrorInfo(ClioError code);

/**
 * @brief Generate JSON from a @ref rpc::WarningCode.
 *
 * @param code The warning code
 * @return The JSON output
 */
boost::json::object
makeWarning(WarningCode code);

/**
 * @brief Generate JSON from a @ref rpc::Status.
 *
 * @param status The status object
 * @return The JSON output
 */
boost::json::object
makeError(Status const& status);

/**
 * @brief Generate JSON from a @ref rpc::RippledError.
 *
 * @param err The rippled error
 * @param customError A custom error
 * @param customMessage A custom message
 * @return The JSON output
 */
boost::json::object
makeError(
    RippledError err,
    std::optional<std::string_view> customError = std::nullopt,
    std::optional<std::string_view> customMessage = std::nullopt
);

/**
 * @brief Generate JSON from a @ref rpc::ClioError.
 *
 * @param err The clio's custom error
 * @param customError A custom error
 * @param customMessage A custom message
 * @return The JSON output
 */
boost::json::object
makeError(
    ClioError err,
    std::optional<std::string_view> customError = std::nullopt,
    std::optional<std::string_view> customMessage = std::nullopt
);

}  // namespace rpc
