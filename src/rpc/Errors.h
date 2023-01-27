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

#include <ripple/protocol/ErrorCodes.h>

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace RPC {

/**
 * @brief Custom clio RPC Errors.
 */
enum class ClioError {
    rpcMALFORMED_CURRENCY = 5000,
    rpcMALFORMED_REQUEST = 5001,
    rpcMALFORMED_OWNER = 5002,
    rpcMALFORMED_ADDRESS = 5003,
};

/**
 * @brief Holds info about a particular @ref ClioError.
 */
struct ClioErrorInfo
{
    ClioError const code;
    std::string_view const error;
    std::string_view const message;
};

/**
 * @brief Clio uses compatible Rippled error codes for most RPC errors.
 */
using RippledError = ripple::error_code_i;

/**
 * @brief Clio operates on a combination of Rippled and Custom Clio error codes.
 *
 * @see RippledError For rippled error codes
 * @see ClioError For custom clio error codes
 */
using CombinedError = std::variant<RippledError, ClioError>;

/**
 * @brief A status returned from any RPC handler.
 */
struct Status
{
    CombinedError code = RippledError::rpcSUCCESS;
    std::string error = "";
    std::string message = "";
    std::optional<boost::json::object> extraInfo;

    Status() = default;
    /* implicit */ Status(CombinedError cde) : code(cde){};
    Status(CombinedError cde, boost::json::object&& xtraInf)
        : code(cde), extraInfo(std::move(xtraInf)){};

    // HACK. Some rippled handlers explicitly specify errors.
    // This means that we have to be able to duplicate this
    // functionality.
    explicit Status(std::string const& msg)
        : code(ripple::rpcUNKNOWN), message(msg)
    {
    }

    Status(CombinedError cde, std::string msg) : code(cde), message(msg)
    {
    }

    Status(CombinedError cde, std::string err, std::string msg)
        : code(cde), error(err), message(msg)
    {
    }

    /**
     * @brief Returns true if the Status is *not* OK.
     */
    operator bool() const
    {
        if (auto err = std::get_if<RippledError>(&code))
            return *err != RippledError::rpcSUCCESS;
        return true;
    }

    /**
     * @brief Returns true if the Status contains the desired @ref RippledError
     *
     * @param other The RippledError to match
     * @return bool true if status matches given error; false otherwise
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
     * @return bool true if status matches given error; false otherwise
     */
    bool
    operator==(ClioError other) const
    {
        if (auto err = std::get_if<ClioError>(&code))
            return *err == other;
        return false;
    }
};

/**
 * @brief Warning codes that can be returned by clio.
 */
enum WarningCode {
    warnUNKNOWN = -1,
    warnRPC_CLIO = 2001,
    warnRPC_OUTDATED = 2002,
    warnRPC_RATE_LIMIT = 2003
};

/**
 * @brief Holds information about a clio warning.
 */
struct WarningInfo
{
    constexpr WarningInfo() = default;
    constexpr WarningInfo(WarningCode cde, char const* msg)
        : code(cde), message(msg)
    {
    }

    WarningCode code = warnUNKNOWN;
    std::string_view const message = "unknown warning";
};

/**
 * @brief Invalid parameters error.
 */
class InvalidParamsError : public std::exception
{
    std::string msg;

public:
    explicit InvalidParamsError(std::string const& m) : msg(m)
    {
    }

    const char*
    what() const throw() override
    {
        return msg.c_str();
    }
};

/**
 * @brief Account not found error.
 */
class AccountNotFoundError : public std::exception
{
    std::string account;

public:
    explicit AccountNotFoundError(std::string const& acct) : account(acct)
    {
    }
    const char*
    what() const throw() override
    {
        return account.c_str();
    }
};

/**
 * @brief A globally available @ref Status that represents a successful state
 */
static Status OK;

/**
 * @brief Get the warning info object from a warning code.
 *
 * @param code The warning code
 * @return WarningInfo const& A reference to the static warning info
 */
WarningInfo const&
getWarningInfo(WarningCode code);

/**
 * @brief Generate JSON from a warning code.
 *
 * @param code The @ref WarningCode
 * @return boost::json::object The JSON output
 */
boost::json::object
makeWarning(WarningCode code);

/**
 * @brief Generate JSON from a @ref Status.
 *
 * @param status The @ref Status
 * @return boost::json::object The JSON output
 */
boost::json::object
makeError(Status const& status);

/**
 * @brief Generate JSON from a @ref RippledError.
 *
 * @param status The rippled @ref RippledError
 * @return boost::json::object The JSON output
 */
boost::json::object
makeError(
    RippledError err,
    std::optional<std::string_view> customError = std::nullopt,
    std::optional<std::string_view> customMessage = std::nullopt);

/**
 * @brief Generate JSON from a @ref ClioError.
 *
 * @param status The clio's custom @ref ClioError
 * @return boost::json::object The JSON output
 */
boost::json::object
makeError(
    ClioError err,
    std::optional<std::string_view> customError = std::nullopt,
    std::optional<std::string_view> customMessage = std::nullopt);

}  // namespace RPC
