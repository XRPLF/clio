//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "rpc/Errors.h"
#include "rpc/JS.h"
#include "util/Assert.h"
#include "web/interface/ConnectionBase.h"

#include <boost/beast/http/status.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/core.h>
#include <ripple/protocol/ErrorCodes.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace web::detail {

/**
 * @brief A helper that attempts to match rippled reporting mode HTTP errors as close as possible.
 */
class ErrorHelper {
    std::shared_ptr<web::ConnectionBase> connection_;
    std::optional<boost::json::object> request_;

public:
    ErrorHelper(
        std::shared_ptr<web::ConnectionBase> const& connection,
        std::optional<boost::json::object> request = std::nullopt
    )
        : connection_{connection}, request_{std::move(request)}
    {
    }

    void
    sendError(rpc::Status const& err) const
    {
        if (connection_->upgraded) {
            connection_->send(boost::json::serialize(composeError(err)));
        } else {
            // Note: a collection of crutches to match rippled output follows
            if (auto const clioCode = std::get_if<rpc::ClioError>(&err.code)) {
                switch (*clioCode) {
                    case rpc::ClioError::rpcINVALID_API_VERSION:
                        connection_->send(
                            std::string{rpc::getErrorInfo(*clioCode).error}, boost::beast::http::status::bad_request
                        );
                        break;
                    case rpc::ClioError::rpcCOMMAND_IS_MISSING:
                        connection_->send("Null method", boost::beast::http::status::bad_request);
                        break;
                    case rpc::ClioError::rpcCOMMAND_IS_EMPTY:
                        connection_->send("method is empty", boost::beast::http::status::bad_request);
                        break;
                    case rpc::ClioError::rpcCOMMAND_NOT_STRING:
                        connection_->send("method is not string", boost::beast::http::status::bad_request);
                        break;
                    case rpc::ClioError::rpcPARAMS_UNPARSEABLE:
                        connection_->send("params unparseable", boost::beast::http::status::bad_request);
                        break;

                    // others are not applicable but we want a compilation error next time we add one
                    case rpc::ClioError::rpcUNKNOWN_OPTION:
                    case rpc::ClioError::rpcMALFORMED_CURRENCY:
                    case rpc::ClioError::rpcMALFORMED_REQUEST:
                    case rpc::ClioError::rpcMALFORMED_OWNER:
                    case rpc::ClioError::rpcMALFORMED_ADDRESS:
                    case rpc::ClioError::rpcINVALID_HOT_WALLET:
                    case rpc::ClioError::rpcFIELD_NOT_FOUND_TRANSACTION:
                        ASSERT(
                            false, "Unknown rpc error code {}", static_cast<int>(*clioCode)
                        );  // this should never happen
                        break;
                }
            } else {
                connection_->send(boost::json::serialize(composeError(err)), boost::beast::http::status::bad_request);
            }
        }
    }

    void
    sendInternalError() const
    {
        connection_->send(
            boost::json::serialize(composeError(rpc::RippledError::rpcINTERNAL)),
            boost::beast::http::status::internal_server_error
        );
    }

    void
    sendNotReadyError() const
    {
        connection_->send(
            boost::json::serialize(composeError(rpc::RippledError::rpcNOT_READY)), boost::beast::http::status::ok
        );
    }

    void
    sendTooBusyError() const
    {
        if (connection_->upgraded) {
            connection_->send(
                boost::json::serialize(rpc::makeError(rpc::RippledError::rpcTOO_BUSY)), boost::beast::http::status::ok
            );
        } else {
            connection_->send(
                boost::json::serialize(rpc::makeError(rpc::RippledError::rpcTOO_BUSY)),
                boost::beast::http::status::service_unavailable
            );
        }
    }

    void
    sendJsonParsingError() const
    {
        if (connection_->upgraded) {
            connection_->send(boost::json::serialize(rpc::makeError(rpc::RippledError::rpcBAD_SYNTAX)));
        } else {
            connection_->send(
                fmt::format("Unable to parse JSON from the request"), boost::beast::http::status::bad_request
            );
        }
    }

    boost::json::object
    composeError(auto const& error) const
    {
        auto e = rpc::makeError(error);

        if (request_) {
            auto const appendFieldIfExist = [&](auto const& field) {
                if (request_->contains(field) and not request_->at(field).is_null())
                    e[field] = request_->at(field);
            };

            appendFieldIfExist(JS(id));

            if (connection_->upgraded)
                appendFieldIfExist(JS(api_version));

            e[JS(request)] = request_.value();
        }

        if (connection_->upgraded) {
            return e;
        }
        return {{JS(result), e}};
    }
};

}  // namespace web::detail
