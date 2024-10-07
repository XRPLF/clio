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

#include "util/NameGenerator.hpp"
#include "web/ng/Request.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace web::ng;
namespace http = boost::beast::http;

struct RequestTest : public ::testing::Test {};

struct RequestMethodTestBundle {
    std::string testName;
    Request request;
    Request::Method expectedMethod;
};

struct RequestMethodTest : RequestTest, ::testing::WithParamInterface<RequestMethodTestBundle> {};

TEST_P(RequestMethodTest, method)
{
    EXPECT_EQ(GetParam().request.method(), GetParam().expectedMethod);
}

INSTANTIATE_TEST_SUITE_P(
    RequestMethodTest,
    RequestMethodTest,
    testing::Values(
        RequestMethodTestBundle{
            .testName = "HttpGet",
            .request = Request{http::request<http::string_body>{http::verb::get, "/", 11}},
            .expectedMethod = Request::Method::GET,
        },
        RequestMethodTestBundle{
            .testName = "HttpPost",
            .request = Request{http::request<http::string_body>{http::verb::post, "/", 11}},
            .expectedMethod = Request::Method::POST,
        },
        RequestMethodTestBundle{
            .testName = "WebSocket",
            .request = Request{"websocket message", Request::HttpHeaders{}},
            .expectedMethod = Request::Method::WEBSOCKET,
        },
        RequestMethodTestBundle{
            .testName = "Unsupported",
            .request = Request{http::request<http::string_body>{http::verb::acl, "/", 11}},
            .expectedMethod = Request::Method::UNSUPPORTED,
        }
    ),
    tests::util::NameGenerator
);

struct RequestIsHttpTestBundle {
    std::string testName;
    Request request;
    bool expectedIsHttp;
};

struct RequestIsHttpTest : RequestTest, testing::WithParamInterface<RequestIsHttpTestBundle> {};

TEST_P(RequestIsHttpTest, isHttp)
{
    EXPECT_EQ(GetParam().request.isHttp(), GetParam().expectedIsHttp);
}

INSTANTIATE_TEST_SUITE_P(
    RequestIsHttpTest,
    RequestIsHttpTest,
    testing::Values(
        RequestIsHttpTestBundle{
            .testName = "HttpRequest",
            .request = Request{http::request<http::string_body>{http::verb::get, "/", 11}},
            .expectedIsHttp = true,
        },
        RequestIsHttpTestBundle{
            .testName = "WebSocketRequest",
            .request = Request{"websocket message", Request::HttpHeaders{}},
            .expectedIsHttp = false,
        }
    ),
    tests::util::NameGenerator
);

struct RequestAsHttpRequestTest : RequestTest {};

TEST_F(RequestAsHttpRequestTest, HttpRequest)
{
    http::request<http::string_body> const httpRequest{http::verb::get, "/some", 11};
    Request const request{httpRequest};
    auto const maybeHttpRequest = request.asHttpRequest();
    ASSERT_TRUE(maybeHttpRequest.has_value());
    auto const& actualHttpRequest = maybeHttpRequest->get();
    EXPECT_EQ(actualHttpRequest.method(), httpRequest.method());
    EXPECT_EQ(actualHttpRequest.target(), httpRequest.target());
    EXPECT_EQ(actualHttpRequest.version(), httpRequest.version());
}

TEST_F(RequestAsHttpRequestTest, WebSocketRequest)
{
    Request const request{"websocket message", Request::HttpHeaders{}};
    auto const maybeHttpRequest = request.asHttpRequest();
    EXPECT_FALSE(maybeHttpRequest.has_value());
}

struct RequestMessageTest : RequestTest {};

TEST_F(RequestMessageTest, HttpRequest)
{
    std::string const body = "some body";
    http::request<http::string_body> const httpRequest{http::verb::post, "/some", 11, body};
    Request const request{httpRequest};
    EXPECT_EQ(request.message(), httpRequest.body());
}

TEST_F(RequestMessageTest, WebSocketRequest)
{
    std::string const message = "websocket message";
    Request const request{message, Request::HttpHeaders{}};
    EXPECT_EQ(request.message(), message);
}

struct RequestTargetTestBundle {
    std::string testName;
    Request request;
    std::optional<std::string> expectedTarget;
};

struct RequestTargetTest : RequestTest, ::testing::WithParamInterface<RequestTargetTestBundle> {};

TEST_P(RequestTargetTest, target)
{
    auto const maybeTarget = GetParam().request.target();
    EXPECT_EQ(maybeTarget, GetParam().expectedTarget);
}

INSTANTIATE_TEST_SUITE_P(
    RequestTargetTest,
    RequestTargetTest,
    testing::Values(
        RequestTargetTestBundle{
            .testName = "HttpRequest",
            .request = Request{http::request<http::string_body>{http::verb::get, "/some", 11}},
            .expectedTarget = "/some",
        },
        RequestTargetTestBundle{
            .testName = "WebSocketRequest",
            .request = Request{"websocket message", Request::HttpHeaders{}},
            .expectedTarget = std::nullopt,
        }
    ),
    tests::util::NameGenerator
);

struct RequestHeaderValueTest : RequestTest {};

TEST_F(RequestHeaderValueTest, headerValue)
{
    http::request<http::string_body> httpRequest{http::verb::get, "/some", 11};
    http::field const headerName = http::field::user_agent;
    std::string const headerValue = "clio";
    httpRequest.set(headerName, headerValue);

    Request const request{httpRequest};
    auto const maybeHeaderValue = request.headerValue(headerName);
    ASSERT_TRUE(maybeHeaderValue.has_value());
    EXPECT_EQ(maybeHeaderValue.value(), headerValue);
}

TEST_F(RequestHeaderValueTest, headerValueString)
{
    http::request<http::string_body> httpRequest{http::verb::get, "/some", 11};
    std::string const headerName = "Custom";
    std::string const headerValue = "some value";
    httpRequest.set(headerName, headerValue);
    Request const request{httpRequest};
    auto const maybeHeaderValue = request.headerValue(headerName);
    ASSERT_TRUE(maybeHeaderValue.has_value());
    EXPECT_EQ(maybeHeaderValue.value(), headerValue);
}

TEST_F(RequestHeaderValueTest, headerValueNotFound)
{
    http::request<http::string_body> httpRequest{http::verb::get, "/some", 11};
    Request const request{httpRequest};
    auto const maybeHeaderValue = request.headerValue(http::field::user_agent);
    EXPECT_FALSE(maybeHeaderValue.has_value());
}

TEST_F(RequestHeaderValueTest, headerValueWebsocketRequest)
{
    Request::HttpHeaders headers;
    http::field const headerName = http::field::user_agent;
    std::string const headerValue = "clio";
    headers.set(headerName, headerValue);
    Request const request{"websocket message", headers};
    auto const maybeHeaderValue = request.headerValue(headerName);
    ASSERT_TRUE(maybeHeaderValue.has_value());
    EXPECT_EQ(maybeHeaderValue.value(), headerValue);
}
