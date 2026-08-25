#pragma once

#include "rpc/Errors.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/system/error_code.hpp>

#include <optional>
#include <string_view>

namespace web {

/**
 * @brief Parse a serialized response body and attach the DOSGuard "load" warning to it.
 *
 * Sets `warning` to "load" and appends a rpc::WarningCode::WarnRpcRateLimit entry to the `warnings`
 * array, creating that array if the body has none - or replacing it if `warnings` is present but is
 * not an array.
 *
 * @note Not every response body is JSON. Over plain HTTP the error paths return text/html bodies
 * (e.g. "Null method" or "Unable to parse JSON from the request"), which cannot carry the warning.
 * For those this returns std::nullopt and the caller must send the body unchanged - parsing them as
 * JSON would throw.
 *
 * @param message The serialized response body
 * @return The response as a JSON object with the warning attached; callers that need a string body
 * must serialize it again. std::nullopt if @p message does not parse as a JSON object.
 */
inline std::optional<boost::json::object>
withLoadWarning(std::string_view message)
{
    boost::system::error_code ec;
    auto const parsed = boost::json::parse(message, ec);
    if (ec.failed() or not parsed.is_object())
        return std::nullopt;

    auto jsonResponse = parsed.as_object();
    jsonResponse["warning"] = "load";

    if (jsonResponse.contains("warnings") and jsonResponse["warnings"].is_array()) {
        jsonResponse["warnings"].as_array().push_back(
            rpc::makeWarning(rpc::WarningCode::WarnRpcRateLimit)
        );
    } else {
        jsonResponse["warnings"] =
            boost::json::array{rpc::makeWarning(rpc::WarningCode::WarnRpcRateLimit)};
    }

    return jsonResponse;
}

}  // namespace web
