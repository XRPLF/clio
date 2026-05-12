#include "rpc/Errors.hpp"
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

struct NgErrorHandlingTests : public virtual ::testing::Test {
    static Request
    makeRequest(bool isHttp, std::optional<std::string> body = std::nullopt)
    {
        if (isHttp) {
            return Request{
                http::request<http::string_body>{http::verb::post, "/", 11, body.value_or("")}
            };
        }
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

struct NgErrorHandlingMakeErrorTest
    : NgErrorHandlingTests,
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
            R"JSON({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})JSON",
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
            "HttpRequest_ParamsUnparsable",
            true,
            rpc::Status{rpc::ClioError::RpcParamsUnparsable},
            "params unparsable",
            boost::beast::http::status::bad_request
        },
        NgErrorHandlingMakeErrorTestBundle{
            "HttpRequest_RippledError",
            true,
            rpc::Status{rpc::RippledError::rpcTOO_BUSY},
            R"JSON({"result":{"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"}})JSON",
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

struct NgErrorHandlingMakeInternalErrorTest
    : NgErrorHandlingTests,
      testing::WithParamInterface<NgErrorHandlingMakeInternalErrorTestBundle> {};

TEST_P(NgErrorHandlingMakeInternalErrorTest, ComposeError)
{
    auto const request = makeRequest(GetParam().isHttp, GetParam().request);
    std::optional<boost::json::object> const requestJson = GetParam().request.has_value()
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
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
             std::string{R"JSON({"id": 1, "api_version": 2})JSON"},
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
             std::string{R"JSON({"api_version": 2})JSON"},
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
             std::string{R"JSON({"id": 1, "api_version": 2})JSON"},
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
            R"JSON({"result":{"error":"notReady","error_code":13,"error_message":"Not ready to handle this request.","status":"error","type":"response"}})JSON"
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
            R"JSON({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})JSON"
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
            R"JSON({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})JSON"
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
            R"JSON({"error":"badSyntax","error_code":1,"error_message":"Syntax error.","status":"error","type":"response"})JSON"
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

struct NgErrorHandlingComposeErrorTest
    : NgErrorHandlingTests,
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
             R"JSON({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response"})JSON"
         },
         NgErrorHandlingComposeErrorTestBundle{
             "NoRequest_HttpConnection",
             true,
             std::nullopt,
             R"JSON({"result":{"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response"}})JSON"
         },
         NgErrorHandlingComposeErrorTestBundle{
             "Request_WebsocketConnection",
             false,
             boost::json::object{{"id", 1}, {"api_version", 2}},
             R"JSON({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","id":1,"api_version":2,"request":{"id":1,"api_version":2}})JSON",
         },
         NgErrorHandlingComposeErrorTestBundle{
             "Request_WebsocketConnection_NoId",
             false,
             boost::json::object{{"api_version", 2}},
             R"JSON({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","api_version":2,"request":{"api_version":2}})JSON",
         },
         NgErrorHandlingComposeErrorTestBundle{
             "Request_HttpConnection",
             true,
             boost::json::object{{"id", 1}, {"api_version", 2}},
             R"JSON({"result":{"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","id":1,"request":{"id":1,"api_version":2}}})JSON"
         }}
    ),
    tests::util::kNAME_GENERATOR
);
