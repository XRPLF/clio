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

#include "rpc/Errors.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/NameGenerator.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/impl/ErrorHandling.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <variant>

using namespace web::ng::impl;
using namespace web::ng;

namespace http = boost::beast::http;

struct NgErrorHandlingTests : NoLoggerFixture {
    static Request
    makeRequest(bool isHttp, std::optional<std::string> body = std::nullopt)
    {
        if (isHttp)
            return Request{http::request<http::string_body>{http::verb::post, "/", 11, body.value_or("")}};
        static Request::HttpHeaders const kHEADERS;
        return Request{body.value_or(""), kHEADERS};
    }
};

struct NgErrorHandlingMakeErrorTestBundle {
    std::string testName;
    bool isHttp;
    rpc::Status status;
    std::string expectedMessage;
    boost::beast::http::status expectedStatus;
};

struct NgErrorHandlingMakeErrorTest : NgErrorHandlingTests,
                                      testing::WithParamInterface<NgErrorHandlingMakeErrorTestBundle> {};

TEST_P(NgErrorHandlingMakeErrorTest, MakeError)
{
    auto const request = makeRequest(GetParam().isHttp);
    ErrorHelper const errorHelper{request};

    auto response = errorHelper.makeError(GetParam().status);
    EXPECT_EQ(response.message(), GetParam().expectedMessage);
    if (GetParam().isHttp) {
        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), GetParam().expectedStatus);

        std::string expectedContentType = "text/html";
        if (std::holds_alternative<rpc::RippledError>(GetParam().status.code))
            expectedContentType = "application/json";

        EXPECT_EQ(httpResponse.at(http::field::content_type), expectedContentType);
    }
}

INSTANTIATE_TEST_CASE_P(
    ng_ErrorHandlingMakeErrorTestGroup,
    NgErrorHandlingMakeErrorTest,
    testing::ValuesIn({
        NgErrorHandlingMakeErrorTestBundle{
            "WsRequest",
            false,
            rpc::Status{rpc::RippledError::rpcTOO_BUSY},
            R"({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})",
            boost::beast::http::status::ok
        },
        NgErrorHandlingMakeErrorTestBundle{
            "HttpRequest_InvalidApiVersion",
            true,
            rpc::Status{rpc::ClioError::RpcInvalidApiVersion},
            "invalid_API_version",
            boost::beast::http::status::bad_request
        },
        NgErrorHandlingMakeErrorTestBundle{
            "HttpRequest_CommandIsMissing",
            true,
            rpc::Status{rpc::ClioError::RpcCommandIsMissing},
            "Null method",
            boost::beast::http::status::bad_request
        },
        NgErrorHandlingMakeErrorTestBundle{
            "HttpRequest_CommandIsEmpty",
            true,
            rpc::Status{rpc::ClioError::RpcCommandIsEmpty},
            "method is empty",
            boost::beast::http::status::bad_request
        },
        NgErrorHandlingMakeErrorTestBundle{
            "HttpRequest_CommandNotString",
            true,
            rpc::Status{rpc::ClioError::RpcCommandNotString},
            "method is not string",
            boost::beast::http::status::bad_request
        },
        NgErrorHandlingMakeErrorTestBundle{
            "HttpRequest_ParamsUnparseable",
            true,
            rpc::Status{rpc::ClioError::RpcParamsUnparseable},
            "params unparseable",
            boost::beast::http::status::bad_request
        },
        NgErrorHandlingMakeErrorTestBundle{
            "HttpRequest_RippledError",
            true,
            rpc::Status{rpc::RippledError::rpcTOO_BUSY},
            R"({"result":{"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"}})",
            boost::beast::http::status::bad_request
        },
    }),
    tests::util::kNAME_GENERATOR
);

struct NgErrorHandlingMakeInternalErrorTestBundle {
    std::string testName;
    bool isHttp;
    std::optional<std::string> request;
    boost::json::object expectedResult;
};

struct NgErrorHandlingMakeInternalErrorTest : NgErrorHandlingTests,
                                              testing::WithParamInterface<NgErrorHandlingMakeInternalErrorTestBundle> {
};

TEST_P(NgErrorHandlingMakeInternalErrorTest, ComposeError)
{
    auto const request = makeRequest(GetParam().isHttp, GetParam().request);
    std::optional<boost::json::object> const requestJson = GetParam().request.has_value()
        ? std::make_optional(boost::json::parse(*GetParam().request).as_object())
        : std::nullopt;
    ErrorHelper const errorHelper{request, requestJson};

    auto response = errorHelper.makeInternalError();

    EXPECT_EQ(response.message(), boost::json::serialize(GetParam().expectedResult));
    if (GetParam().isHttp) {
        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), boost::beast::http::status::internal_server_error);
        EXPECT_EQ(httpResponse.at(http::field::content_type), "application/json");
    }
}

INSTANTIATE_TEST_CASE_P(
    ng_ErrorHandlingComposeErrorTestGroup,
    NgErrorHandlingMakeInternalErrorTest,
    testing::ValuesIn(
        {NgErrorHandlingMakeInternalErrorTestBundle{
             "NoRequest_WebsocketConnection",
             false,
             std::nullopt,
             {{"error", "internal"},
              {"error_code", 73},
              {"error_message", "Internal error."},
              {"status", "error"},
              {"type", "response"}}
         },
         NgErrorHandlingMakeInternalErrorTestBundle{
             "NoRequest_HttpConnection",
             true,
             std::nullopt,
             {{"result",
               {{"error", "internal"},
                {"error_code", 73},
                {"error_message", "Internal error."},
                {"status", "error"},
                {"type", "response"}}}}
         },
         NgErrorHandlingMakeInternalErrorTestBundle{
             "Request_WebsocketConnection",
             false,
             std::string{R"({"id": 1, "api_version": 2})"},
             {{"error", "internal"},
              {"error_code", 73},
              {"error_message", "Internal error."},
              {"status", "error"},
              {"type", "response"},
              {"id", 1},
              {"api_version", 2},
              {"request", {{"id", 1}, {"api_version", 2}}}}
         },
         NgErrorHandlingMakeInternalErrorTestBundle{
             "Request_WebsocketConnection_NoId",
             false,
             std::string{R"({"api_version": 2})"},
             {{"error", "internal"},
              {"error_code", 73},
              {"error_message", "Internal error."},
              {"status", "error"},
              {"type", "response"},
              {"api_version", 2},
              {"request", {{"api_version", 2}}}}
         },
         NgErrorHandlingMakeInternalErrorTestBundle{
             "Request_HttpConnection",
             true,
             std::string{R"({"id": 1, "api_version": 2})"},
             {{"result",
               {{"error", "internal"},
                {"error_code", 73},
                {"error_message", "Internal error."},
                {"status", "error"},
                {"type", "response"},
                {"id", 1},
                {"request", {{"id", 1}, {"api_version", 2}}}}}}
         }}
    ),
    tests::util::kNAME_GENERATOR
);

TEST_F(NgErrorHandlingTests, MakeNotReadyError)
{
    auto const request = makeRequest(true);
    auto response = ErrorHelper{request}.makeNotReadyError();
    EXPECT_EQ(
        response.message(),
        std::string{
            R"({"result":{"error":"notReady","error_code":13,"error_message":"Not ready to handle this request.","status":"error","type":"response"}})"
        }
    );
    auto const httpResponse = std::move(response).intoHttpResponse();
    EXPECT_EQ(httpResponse.result(), http::status::ok);
    EXPECT_EQ(httpResponse.at(http::field::content_type), "application/json");
}

TEST_F(NgErrorHandlingTests, MakeTooBusyError_WebsocketRequest)
{
    auto const request = makeRequest(false);
    auto response = ErrorHelper{request}.makeTooBusyError();
    EXPECT_EQ(
        response.message(),
        std::string{
            R"({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})"
        }
    );
}

TEST_F(NgErrorHandlingTests, sendTooBusyError_HttpConnection)
{
    auto const request = makeRequest(true);
    auto response = ErrorHelper{request}.makeTooBusyError();
    EXPECT_EQ(
        response.message(),
        std::string{
            R"({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})"
        }
    );
    auto const httpResponse = std::move(response).intoHttpResponse();
    EXPECT_EQ(httpResponse.result(), boost::beast::http::status::service_unavailable);
    EXPECT_EQ(httpResponse.at(http::field::content_type), "application/json");
}

TEST_F(NgErrorHandlingTests, makeJsonParsingError_WebsocketConnection)
{
    auto const request = makeRequest(false);
    auto response = ErrorHelper{request}.makeJsonParsingError();
    EXPECT_EQ(
        response.message(),
        std::string{
            R"({"error":"badSyntax","error_code":1,"error_message":"Syntax error.","status":"error","type":"response"})"
        }
    );
}

TEST_F(NgErrorHandlingTests, makeJsonParsingError_HttpConnection)
{
    auto const request = makeRequest(true);
    auto response = ErrorHelper{request}.makeJsonParsingError();
    EXPECT_EQ(response.message(), std::string{"Unable to parse JSON from the request"});
    auto const httpResponse = std::move(response).intoHttpResponse();
    EXPECT_EQ(httpResponse.result(), boost::beast::http::status::bad_request);
    EXPECT_EQ(httpResponse.at(http::field::content_type), "text/html");
}

struct NgErrorHandlingComposeErrorTestBundle {
    std::string testName;
    bool isHttp;
    std::optional<boost::json::object> request;
    std::string expectedMessage;
};

struct NgErrorHandlingComposeErrorTest : NgErrorHandlingTests,
                                         testing::WithParamInterface<NgErrorHandlingComposeErrorTestBundle> {};

TEST_P(NgErrorHandlingComposeErrorTest, ComposeError)
{
    auto const request = makeRequest(GetParam().isHttp);
    ErrorHelper const errorHelper{request, GetParam().request};
    auto const response = errorHelper.composeError(rpc::Status{rpc::RippledError::rpcINTERNAL});
    EXPECT_EQ(boost::json::serialize(response), GetParam().expectedMessage);
}

INSTANTIATE_TEST_CASE_P(
    ng_ErrorHandlingComposeErrorTestGroup,
    NgErrorHandlingComposeErrorTest,
    testing::ValuesIn(
        {NgErrorHandlingComposeErrorTestBundle{
             "NoRequest_WebsocketConnection",
             false,
             std::nullopt,
             R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response"})"
         },
         NgErrorHandlingComposeErrorTestBundle{
             "NoRequest_HttpConnection",
             true,
             std::nullopt,
             R"({"result":{"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response"}})"
         },
         NgErrorHandlingComposeErrorTestBundle{
             "Request_WebsocketConnection",
             false,
             boost::json::object{{"id", 1}, {"api_version", 2}},
             R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","id":1,"api_version":2,"request":{"id":1,"api_version":2}})",
         },
         NgErrorHandlingComposeErrorTestBundle{
             "Request_WebsocketConnection_NoId",
             false,
             boost::json::object{{"api_version", 2}},
             R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","api_version":2,"request":{"api_version":2}})",
         },
         NgErrorHandlingComposeErrorTestBundle{
             "Request_HttpConnection",
             true,
             boost::json::object{{"id", 1}, {"api_version", 2}},
             R"({"result":{"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","id":1,"request":{"id":1,"api_version":2}}})"
         }}
    ),
    tests::util::kNAME_GENERATOR
);
