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

#include "util/Taggable.hpp"
#include "util/build/Build.hpp"
#include "util/config/Config.hpp"
#include "web/ng/MockConnection.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace web::ng;
namespace http = boost::beast::http;

struct ResponseDeathTest : testing::Test {};

TEST_F(ResponseDeathTest, intoHttpResponseWithoutHttpData)
{
    Request::HttpHeaders const headers{};
    Request const request{"some message", headers};
    Response response{boost::beast::http::status::ok, "message", request};
    EXPECT_DEATH(std::move(response).intoHttpResponse(), "");
}

TEST_F(ResponseDeathTest, asConstBufferWithHttpData)
{
    Request const request{http::request<http::string_body>{http::verb::get, "/", 11}};
    Response const response{boost::beast::http::status::ok, "message", request};
    EXPECT_DEATH(response.asWsResponse(), "");
}

struct ResponseTest : testing::Test {
    int const httpVersion_ = 11;
    http::status const responseStatus_ = http::status::ok;
    Request::HttpHeaders const headers_;
};

TEST_F(ResponseTest, intoHttpResponse)
{
    Request const request{http::request<http::string_body>{http::verb::post, "/", httpVersion_, "some message"}};
    std::string const responseMessage = "response message";

    Response response{responseStatus_, responseMessage, request};

    auto const httpResponse = std::move(response).intoHttpResponse();
    EXPECT_EQ(httpResponse.result(), responseStatus_);
    EXPECT_EQ(httpResponse.body(), responseMessage);
    EXPECT_EQ(httpResponse.version(), httpVersion_);
    EXPECT_EQ(httpResponse.keep_alive(), request.asHttpRequest()->get().keep_alive());

    ASSERT_GT(httpResponse.count(http::field::content_type), 0);
    EXPECT_EQ(httpResponse[http::field::content_type], "text/html");

    ASSERT_GT(httpResponse.count(http::field::content_type), 0);
    EXPECT_EQ(httpResponse[http::field::server], fmt::format("clio-server-{}", util::build::getClioVersionString()));
}

TEST_F(ResponseTest, intoHttpResponseJson)
{
    Request const request{http::request<http::string_body>{http::verb::post, "/", httpVersion_, "some message"}};
    boost::json::object const responseMessage{{"key", "value"}};

    Response response{responseStatus_, responseMessage, request};

    auto const httpResponse = std::move(response).intoHttpResponse();
    EXPECT_EQ(httpResponse.result(), responseStatus_);
    EXPECT_EQ(httpResponse.body(), boost::json::serialize(responseMessage));
    EXPECT_EQ(httpResponse.version(), httpVersion_);
    EXPECT_EQ(httpResponse.keep_alive(), request.asHttpRequest()->get().keep_alive());

    ASSERT_GT(httpResponse.count(http::field::content_type), 0);
    EXPECT_EQ(httpResponse[http::field::content_type], "application/json");

    ASSERT_GT(httpResponse.count(http::field::content_type), 0);
    EXPECT_EQ(httpResponse[http::field::server], fmt::format("clio-server-{}", util::build::getClioVersionString()));
}

TEST_F(ResponseTest, asConstBuffer)
{
    Request const request("some request", headers_);
    std::string const responseMessage = "response message";
    Response const response{responseStatus_, responseMessage, request};

    auto const buffer = response.asWsResponse();
    EXPECT_EQ(buffer.size(), responseMessage.size());

    std::string const messageFromBuffer{static_cast<char const*>(buffer.data()), buffer.size()};
    EXPECT_EQ(messageFromBuffer, responseMessage);
}

TEST_F(ResponseTest, asConstBufferJson)
{
    Request const request("some request", headers_);
    boost::json::object const responseMessage{{"key", "value"}};
    Response const response{responseStatus_, responseMessage, request};

    auto const buffer = response.asWsResponse();
    EXPECT_EQ(buffer.size(), boost::json::serialize(responseMessage).size());

    std::string const messageFromBuffer{static_cast<char const*>(buffer.data()), buffer.size()};
    EXPECT_EQ(messageFromBuffer, boost::json::serialize(responseMessage));
}

TEST_F(ResponseTest, createFromStringAndConnection)
{
    util::TagDecoratorFactory const tagDecoratorFactory{util::Config{}};
    StrictMockConnection const connection{"some ip", boost::beast::flat_buffer{}, tagDecoratorFactory};
    std::string const responseMessage = "response message";

    EXPECT_CALL(connection, wasUpgraded()).WillOnce(testing::Return(false));
    Response response{responseStatus_, responseMessage, connection};

    EXPECT_EQ(response.message(), responseMessage);
    auto const httpResponse = std::move(response).intoHttpResponse();
    EXPECT_EQ(httpResponse.result(), responseStatus_);
    auto const it = httpResponse.find(http::field::content_type);
    ASSERT_NE(it, httpResponse.end());
    EXPECT_EQ(it->value(), "text/html");
}

TEST_F(ResponseTest, createFromJsonAndConnection)
{
    util::TagDecoratorFactory const tagDecoratorFactory{util::Config{}};
    StrictMockConnection const connection{"some ip", boost::beast::flat_buffer{}, tagDecoratorFactory};
    boost::json::object const responseMessage{{"key", "value"}};

    EXPECT_CALL(connection, wasUpgraded()).WillOnce(testing::Return(false));
    Response response{responseStatus_, responseMessage, connection};

    EXPECT_EQ(response.message(), boost::json::serialize(responseMessage));
    auto const httpResponse = std::move(response).intoHttpResponse();
    EXPECT_EQ(httpResponse.result(), responseStatus_);
    auto const it = httpResponse.find(http::field::content_type);
    ASSERT_NE(it, httpResponse.end());
    EXPECT_EQ(it->value(), "application/json");
}

TEST_F(ResponseTest, setMessageString_HttpResponse)
{
    Request const request{http::request<http::string_body>{http::verb::post, "/", httpVersion_, "some request"}};
    Response response{boost::beast::http::status::ok, "message", request};

    std::string const newMessage = "new message";
    response.setMessage(newMessage);

    EXPECT_EQ(response.message(), newMessage);
    auto const httpResponse = std::move(response).intoHttpResponse();
    auto it = httpResponse.find(http::field::content_type);
    ASSERT_NE(it, httpResponse.end());
    EXPECT_EQ(it->value(), "text/html");
}

TEST_F(ResponseTest, setMessageString_WsResponse)
{
    Request const request{"some request", headers_};
    Response response{boost::beast::http::status::ok, "message", request};

    std::string const newMessage = "new message";
    response.setMessage(newMessage);

    EXPECT_EQ(response.message(), newMessage);
}

TEST_F(ResponseTest, setMessageJson_HttpResponse)
{
    Request const request{http::request<http::string_body>{http::verb::post, "/", httpVersion_, "some request"}};
    Response response{boost::beast::http::status::ok, "message", request};

    boost::json::object const newMessage{{"key", "value"}};
    response.setMessage(newMessage);

    auto const httpResponse = std::move(response).intoHttpResponse();
    auto it = httpResponse.find(http::field::content_type);
    ASSERT_NE(it, httpResponse.end());
    EXPECT_EQ(it->value(), "application/json");
}

TEST_F(ResponseTest, setMessageJson_WsResponse)
{
    Request const request{"some request", headers_};
    Response response{boost::beast::http::status::ok, "message", request};

    boost::json::object const newMessage{{"key", "value"}};
    response.setMessage(newMessage);

    EXPECT_EQ(response.message(), boost::json::serialize(newMessage));
}
