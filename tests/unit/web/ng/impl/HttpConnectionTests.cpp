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

#include "util/AsioContextTestFixture.hpp"
#include "util/Taggable.hpp"
#include "util/TestHttpServer.hpp"
#include "util/config/Config.hpp"
#include "util/requests/RequestBuilder.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/impl/HttpConnection.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/json/object.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <utility>

using namespace web::ng::impl;

struct HttpConnectionTests : SyncAsioContextTest {
    util::TagDecoratorFactory tagDecoratorFactory_{util::Config{boost::json::object{{"log_tag_style", "int"}}}};
    TestHttpServer httpServer_{ctx, "0.0.0.0"};

    PlainHttpConnection
    acceptConnection(boost::asio::yield_context yield)
    {
        auto expectedSocket = httpServer_.accept(yield);
        [&]() { ASSERT_TRUE(expectedSocket.has_value()) << expectedSocket.error().message(); }();
        auto ip = expectedSocket->remote_endpoint().address().to_string();
        return PlainHttpConnection{
            std::move(expectedSocket).value(), std::move(ip), boost::beast::flat_buffer{}, tagDecoratorFactory_
        };
    }

    // PlainHttpConnection
    // connect(std::string host, std::string port, boost::asio::yield_context yield)
    // {
    // }
};

TEST_F(HttpConnectionTests, plainConnectionSendReceive)
{
    boost::asio::spawn([this](boost::asio::yield_context yield) {
        auto maybeError =
            util::requests::RequestBuilder{"localhost", httpServer_.port()}.addData("some data").postPlain(yield);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        auto expectedRequest = connection.receive(yield, std::chrono::milliseconds{1});
        ASSERT_TRUE(expectedRequest.has_value()) << expectedRequest.error().message();
        EXPECT_EQ(expectedRequest->method(), web::ng::Request::Method::POST);
        EXPECT_EQ(expectedRequest->message(), "some data");
    });
}
