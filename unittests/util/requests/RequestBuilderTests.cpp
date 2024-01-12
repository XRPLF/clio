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

#include "util/Fixtures.h"
#include "util/TestHttpServer.h"
#include "util/requests/RequestBuilder.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <optional>

using namespace util::requests;
using namespace boost;
using namespace boost::beast;

struct RequestBuilderTest : SyncAsioContextTest {
    TestHttpServer server{ctx, "0.0.0.0", 11111};
    RequestBuilder builder{"localhost", "11111"};
};

TEST_F(RequestBuilderTest, testGetRequest)
{
    server.handleRequest(
        [](http::request<http::string_body> request) -> std::optional<http::response<http::string_body>> {
            [&]() {
                ASSERT_TRUE(request.target() == "/");
                ASSERT_TRUE(request.method() == http::verb::get);
            }();
            return http::response<http::string_body>{http::status::ok, 11, "Hello, world!"};
        }
    );

    runSpawn([this](asio::yield_context yield) {
        auto response = builder.get(yield);
        ASSERT_TRUE(response) << response.error().message;
        EXPECT_EQ(response.value(), "Hello, world!");
    });
}
