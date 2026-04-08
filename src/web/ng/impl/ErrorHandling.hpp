#pragma once

#include "rpc/Errors.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/format.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/jss.h>

#include <functional>
#include <optional>

namespace web::ng::impl {

/**
 * @brief A helper that attempts to match rippled reporting mode HTTP errors as close as possible.
 */
class ErrorHelper {
    std::reference_wrapper<Request const> rawRequest_;
    std::optional<boost::json::object> request_;

public:
    /**
     * @brief Construct a new Error Helper object
     *
     * @param rawRequest The request that caused the error.
     * @param request The parsed request that caused the error.
     */
    ErrorHelper(
        Request const& rawRequest,
        std::optional<boost::json::object> request = std::nullopt
    );

    /**
     * @brief Make an error response from a status.
     *
     * @param err The status to make an error response from.
     * @return
     */
    [[nodiscard]] Response
    makeError(rpc::Status const& err) const;

    /**
     * @brief Make an internal error response.
     *
     * @return A response with an internal error.
     */
    [[nodiscard]] Response
    makeInternalError() const;

    /**
     * @brief Make a response for when the server is not ready.
     *
     * @return A response with a not ready error.
     */
    [[nodiscard]] Response
    makeNotReadyError() const;

    /**
     * @brief Make a response for when the server is too busy.
     *
     * @return A response with a too busy error.
     */
    [[nodiscard]] Response
    makeTooBusyError() const;

    /**
     * @brief Make a response when json parsing fails.
     *
     * @return A response with a json parsing error.
     */
    [[nodiscard]] Response
    makeJsonParsingError() const;

    /**
     * @brief Compose an error into json object from a status.
     *
     * @param error The status to compose into a json object.
     * @return The composed json object.
     */
    [[nodiscard]] boost::json::object
    composeError(rpc::Status const& error) const;

    /**
     * @brief Compose an error into json object from a rippled error.
     *
     * @param error The rippled error to compose into a json object.
     * @return The composed json object.
     */
    [[nodiscard]] boost::json::object
    composeError(rpc::RippledError error) const;
};

}  // namespace web::ng::impl
