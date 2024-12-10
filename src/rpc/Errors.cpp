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

#include "rpc/Errors.hpp"

#include "rpc/JS.hpp"
#include "util/OverloadSet.hpp"

#include <boost/json/object.hpp>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

using namespace std;

namespace rpc {

WarningInfo const&
getWarningInfo(WarningCode code)
{
    constexpr static WarningInfo infos[]{
        {warnUNKNOWN, "Unknown warning"},
        {warnRPC_CLIO,
         "This is a clio server. clio only serves validated data. If you want to talk to rippled, include "
         "'ledger_index':'current' in your request"},
        {warnRPC_OUTDATED, "This server may be out of date"},
        {warnRPC_RATE_LIMIT, "You are about to be rate limited"},
        {warnRPC_DEPRECATED,
         "Some fields from your request are deprecated. Please check the documentation at "
         "https://xrpl.org/docs/references/http-websocket-apis/ and update your request."}
    };

    auto matchByCode = [code](auto const& info) { return info.code == code; };
    if (auto it = std::ranges::find_if(infos, matchByCode); it != end(infos))
        return *it;

    throw(out_of_range("Invalid WarningCode"));
}

boost::json::object
makeWarning(WarningCode code)
{
    auto json = boost::json::object{};
    auto const& info = getWarningInfo(code);
    json["id"] = code;
    json["message"] = info.message;
    return json;
}

ClioErrorInfo const&
getErrorInfo(ClioError code)
{
    constexpr static ClioErrorInfo infos[]{
        {.code = ClioError::rpcMALFORMED_CURRENCY, .error = "malformedCurrency", .message = "Malformed currency."},
        {.code = ClioError::rpcMALFORMED_REQUEST, .error = "malformedRequest", .message = "Malformed request."},
        {.code = ClioError::rpcMALFORMED_OWNER, .error = "malformedOwner", .message = "Malformed owner."},
        {.code = ClioError::rpcMALFORMED_ADDRESS, .error = "malformedAddress", .message = "Malformed address."},
        {.code = ClioError::rpcINVALID_HOT_WALLET, .error = "invalidHotWallet", .message = "Invalid hot wallet."},
        {.code = ClioError::rpcUNKNOWN_OPTION, .error = "unknownOption", .message = "Unknown option."},
        {.code = ClioError::rpcFIELD_NOT_FOUND_TRANSACTION,
         .error = "fieldNotFoundTransaction",
         .message = "Missing field."},
        {.code = ClioError::rpcMALFORMED_ORACLE_DOCUMENT_ID,
         .error = "malformedDocumentID",
         .message = "Malformed oracle_document_id."},
        {.code = ClioError::rpcMALFORMED_AUTHORIZED_CREDENTIALS,
         .error = "malformedAuthorizedCredentials",
         .message = "Malformed authorized credentials."},
        // special system errors
        {.code = ClioError::rpcINVALID_API_VERSION, .error = JS(invalid_API_version), .message = "Invalid API version."
        },
        {.code = ClioError::rpcCOMMAND_IS_MISSING,
         .error = JS(missingCommand),
         .message = "Method is not specified or is not a string."},
        {.code = ClioError::rpcCOMMAND_NOT_STRING, .error = "commandNotString", .message = "Method is not a string."},
        {.code = ClioError::rpcCOMMAND_IS_EMPTY, .error = "emptyCommand", .message = "Method is an empty string."},
        {.code = ClioError::rpcPARAMS_UNPARSEABLE,
         .error = "paramsUnparseable",
         .message = "Params must be an array holding exactly one object."},
        // etl related errors
        {.code = ClioError::etlCONNECTION_ERROR, .error = "connectionError", .message = "Couldn't connect to rippled."},
        {.code = ClioError::etlREQUEST_ERROR, .error = "requestError", .message = "Error sending request to rippled."},
        {.code = ClioError::etlREQUEST_TIMEOUT, .error = "timeout", .message = "Request to rippled timed out."},
        {.code = ClioError::etlINVALID_RESPONSE,
         .error = "invalidResponse",
         .message = "Rippled returned an invalid response."}
    };

    auto matchByCode = [code](auto const& info) { return info.code == code; };
    if (auto it = std::ranges::find_if(infos, matchByCode); it != end(infos))
        return *it;

    throw(out_of_range("Invalid error code"));
}

boost::json::object
makeError(RippledError err, std::optional<std::string_view> customError, std::optional<std::string_view> customMessage)
{
    boost::json::object json;
    auto const& info = ripple::RPC::get_error_info(err);

    json["error"] = customError.value_or(info.token.c_str()).data();
    json["error_code"] = static_cast<uint32_t>(err);
    json["error_message"] = customMessage.value_or(info.message.c_str()).data();
    json["status"] = "error";
    json["type"] = "response";

    return json;
}

boost::json::object
makeError(ClioError err, std::optional<std::string_view> customError, std::optional<std::string_view> customMessage)
{
    boost::json::object json;
    auto const& info = getErrorInfo(err);

    json["error"] = customError.value_or(info.error);
    json["error_code"] = static_cast<uint32_t>(info.code);
    json["error_message"] = customMessage.value_or(info.message);
    json["status"] = "error";
    json["type"] = "response";

    return json;
}

boost::json::object
makeError(Status const& status)
{
    auto wrapOptional = [](string_view const& str) { return str.empty() ? nullopt : make_optional(str); };

    auto res = visit(
        util::OverloadSet{
            [&status, &wrapOptional](RippledError err) {
                if (err == ripple::rpcUNKNOWN)
                    return boost::json::object{{"error", status.message}, {"type", "response"}, {"status", "error"}};

                return makeError(err, wrapOptional(status.error), wrapOptional(status.message));
            },
            [&status, &wrapOptional](ClioError err) {
                return makeError(err, wrapOptional(status.error), wrapOptional(status.message));
            },
        },
        status.code
    );

    if (status.extraInfo) {
        for (auto& [key, value] : status.extraInfo.value())
            res[key] = value;
    }

    return res;
}

}  // namespace rpc
