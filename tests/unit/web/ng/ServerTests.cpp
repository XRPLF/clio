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
#include "util/LoggerFixtures.hpp"
#include "util/NameGenerator.hpp"
#include "util/config/Config.hpp"
#include "web/ng/Server.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using namespace web::ng;

struct MakeServerTestBundle {
    std::string testName;
    std::string configJson;
    bool expectSuccess;
};

struct MakeServerTest : NoLoggerFixture, testing::WithParamInterface<MakeServerTestBundle> {
    boost::asio::io_context ioContext_;
};

TEST_P(MakeServerTest, Make)
{
    util::Config const config{boost::json::parse(GetParam().configJson)};
    auto const expectedServer = make_Server(config, ioContext_);
    EXPECT_EQ(expectedServer.has_value(), GetParam().expectSuccess);
}

INSTANTIATE_TEST_CASE_P(
    MakeServerTests,
    MakeServerTest,
    testing::Values(
        MakeServerTestBundle{
            "BadEndpoint",
            R"json(
                {
                    "server": {"ip": "wrong", "port": 12345}
                }
            )json",
            false
        },
        MakeServerTestBundle{
            "PortMissing",
            R"json(
        {
            "server": {"ip": "127.0.0.1"}
        }
            )json",
            false
        },
        MakeServerTestBundle{
            "BadSslConfig",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345},
            "ssl_cert_file": "somг_file"
        }
            )json",
            false
        },
        MakeServerTestBundle{
            "BadProcessingPolicy",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345, "processing_policy": "wrong"}
        }
            )json",
            false
        },
        MakeServerTestBundle{
            "CorrectConfig_ParallelPolicy",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345, "processing_policy": "parallel"}
        }
            )json",
            true
        },
        MakeServerTestBundle{
            "CorrectConfig_SequentPolicy",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345, "processing_policy": "sequent"}
        }
            )json",
            true
        }
    ),
    tests::util::NameGenerator
);

struct ServerTest : SyncAsioContextTest {};
