/** @file */
#pragma once

#include <boost/json/object.hpp>
#include <rpcspec/Errors.hpp>

#include <optional>
#include <string_view>

namespace rpc {

/**
 * @brief Holds info about a particular ClioError.
 */
struct ClioErrorInfo {
    ClioError const code;
    std::string_view const error;
    std::string_view const message;
};

/**
 * @brief A globally available rpc::Status that represents a successful state.
 */
static Status gOk;

/**
 * @brief Get the error info object from an clio-specific error code.
 *
 * @param code The error code
 * @return A reference to the static error info
 */
ClioErrorInfo const&
getErrorInfo(ClioError code);

/**
 * @brief Generate JSON from a rpc::Status.
 *
 * @param status The status object
 * @return The JSON output
 */
boost::json::object
makeError(Status const& status);

/**
 * @brief Generate JSON from a rpc::RippledError.
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
 * @brief Generate JSON from a rpc::ClioError.
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
