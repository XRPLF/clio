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

#include "web/ng/impl/ErrorHandling.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "util/Assert.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/json/object.hpp>
#include <fmt/core.h>
#include <xrpl/protocol/jss.h>

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace http = boost::beast::http;

namespace web::ng::impl {

namespace {

boost::json::object
composeErrorImpl(auto const& error, Request const& rawRequest, std::optional<boost::json::object> const& request)
{
    auto e = rpc::makeError(error);

    if (request) {
        auto const appendFieldIfExist = [&](auto const& field) {
            if (request->contains(field) and not request->at(field).is_null())
                e[field] = request->at(field);
        };

        appendFieldIfExist(JS(id));

        if (not rawRequest.isHttp())
            appendFieldIfExist(JS(api_version));

        e[JS(request)] = request.value();
    }

    if (not rawRequest.isHttp()) {
        return e;
    }
    return {{JS(result), e}};
}

}  // namespace

ErrorHelper::ErrorHelper(Request const& rawRequest, std::optional<boost::json::object> request)
    : rawRequest_{rawRequest}, request_{std::move(request)}
{
}

Response
ErrorHelper::makeError(rpc::Status const& err) const
{
    if (not rawRequest_.get().isHttp()) {
        return Response{http::status::bad_request, composeError(err), rawRequest_};
    }

    // Note: a collection of crutches to match rippled output follows
    if (auto const clioCode = std::get_if<rpc::ClioError>(&err.code)) {
        switch (*clioCode) {
            case rpc::ClioError::RpcInvalidApiVersion:
                return Response{
                    http::status::bad_request, std::string{rpc::getErrorInfo(*clioCode).error}, rawRequest_
                };
            case rpc::ClioError::RpcCommandIsMissing:
                return Response{http::status::bad_request, "Null method", rawRequest_};
            case rpc::ClioError::RpcCommandIsEmpty:
                return Response{http::status::bad_request, "method is empty", rawRequest_};
            case rpc::ClioError::RpcCommandNotString:
                return Response{http::status::bad_request, "method is not string", rawRequest_};
            case rpc::ClioError::RpcParamsUnparseable:
                return Response{http::status::bad_request, "params unparseable", rawRequest_};

            // others are not applicable but we want a compilation error next time we add one
            case rpc::ClioError::RpcUnknownOption:
            case rpc::ClioError::RpcMalformedCurrency:
            case rpc::ClioError::RpcMalformedRequest:
            case rpc::ClioError::RpcMalformedOwner:
            case rpc::ClioError::RpcMalformedAddress:
            case rpc::ClioError::RpcInvalidHotWallet:
            case rpc::ClioError::RpcFieldNotFoundTransaction:
            case rpc::ClioError::RpcMalformedOracleDocumentId:
            case rpc::ClioError::RpcMalformedAuthorizedCredentials:
            case rpc::ClioError::EtlConnectionError:
            case rpc::ClioError::EtlRequestError:
            case rpc::ClioError::EtlRequestTimeout:
            case rpc::ClioError::EtlInvalidResponse:
                ASSERT(false, "Unknown rpc error code {}", static_cast<int>(*clioCode));  // this should never happen
                break;
        }
    }

    return Response{http::status::bad_request, composeError(err), rawRequest_};
}

Response
ErrorHelper::makeInternalError() const
{
    return Response{http::status::internal_server_error, composeError(rpc::RippledError::rpcINTERNAL), rawRequest_};
}

Response
ErrorHelper::makeNotReadyError() const
{
    return Response{http::status::ok, composeError(rpc::RippledError::rpcNOT_READY), rawRequest_};
}

Response
ErrorHelper::makeTooBusyError() const
{
    if (not rawRequest_.get().isHttp()) {
        return Response{http::status::too_many_requests, rpc::makeError(rpc::RippledError::rpcTOO_BUSY), rawRequest_};
    }

    return Response{http::status::service_unavailable, rpc::makeError(rpc::RippledError::rpcTOO_BUSY), rawRequest_};
}

Response
ErrorHelper::makeJsonParsingError() const
{
    if (not rawRequest_.get().isHttp()) {
        return Response{http::status::bad_request, rpc::makeError(rpc::RippledError::rpcBAD_SYNTAX), rawRequest_};
    }

    return Response{http::status::bad_request, fmt::format("Unable to parse JSON from the request"), rawRequest_};
}

boost::json::object
ErrorHelper::composeError(rpc::Status const& error) const
{
    return composeErrorImpl(error, rawRequest_, request_);
}

boost::json::object
ErrorHelper::composeError(rpc::RippledError error) const
{
    return composeErrorImpl(error, rawRequest_, request_);
}

}  // namespace web::ng::impl
