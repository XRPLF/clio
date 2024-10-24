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

#include "util/LoggerFixtures.hpp"
#include "util/config/Config.hpp"
#include "web/impl/AdminVerificationStrategy.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace http = boost::beast::http;

class IPAdminVerificationStrategyTest : public NoLoggerFixture {
protected:
    web::impl::IPAdminVerificationStrategy strat_;
    http::request<http::string_body> request_;
};

TEST_F(IPAdminVerificationStrategyTest, IsAdminOnlyForIP_127_0_0_1)
{
    EXPECT_TRUE(strat_.isAdmin(request_, "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(request_, "127.0.0.2"));
    EXPECT_FALSE(strat_.isAdmin(request_, "127"));
    EXPECT_FALSE(strat_.isAdmin(request_, ""));
    EXPECT_FALSE(strat_.isAdmin(request_, "localhost"));
}

class PasswordAdminVerificationStrategyTest : public NoLoggerFixture {
protected:
    std::string const password_ = "secret";
    std::string const passwordHash_ = "2bb80d537b1da3e38bd30361aa855686bde0eacd7162fef6a25fe97bf527a25b";

    web::impl::PasswordAdminVerificationStrategy strat_{password_};

    static http::request<http::string_body>
    makeRequest(std::string const& password, http::field const field = http::field::authorization)
    {
        http::request<http::string_body> request = {};
        request.set(field, "Password " + password);
        return request;
    }
};

TEST_F(PasswordAdminVerificationStrategyTest, IsAdminReturnsTrueOnlyForValidPasswordInAuthHeader)
{
    EXPECT_TRUE(strat_.isAdmin(makeRequest(passwordHash_), ""));
    EXPECT_TRUE(strat_.isAdmin(makeRequest(passwordHash_), "123"));

    // Wrong password
    EXPECT_FALSE(strat_.isAdmin(makeRequest("SECRET"), ""));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("SECRET"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("S"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("SeCret"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("secre"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("s"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("a"), "127.0.0.1"));

    // Wrong header
    EXPECT_FALSE(strat_.isAdmin(makeRequest(passwordHash_, http::field::authentication_info), ""));
}

struct MakeAdminVerificationStrategyTestParams {
    std::string testName;
    std::optional<std::string> passwordOpt;
    bool expectIpStrategy;
    bool expectPasswordStrategy;
};

class MakeAdminVerificationStrategyTest : public testing::TestWithParam<MakeAdminVerificationStrategyTestParams> {};

TEST_P(MakeAdminVerificationStrategyTest, ChoosesStrategyCorrectly)
{
    auto strat = web::impl::make_AdminVerificationStrategy(GetParam().passwordOpt);
    auto ipStrat = dynamic_cast<web::impl::IPAdminVerificationStrategy*>(strat.get());
    EXPECT_EQ(ipStrat != nullptr, GetParam().expectIpStrategy);
    auto passwordStrat = dynamic_cast<web::impl::PasswordAdminVerificationStrategy*>(strat.get());
    EXPECT_EQ(passwordStrat != nullptr, GetParam().expectPasswordStrategy);
}

INSTANTIATE_TEST_CASE_P(
    MakeAdminVerificationStrategyTest,
    MakeAdminVerificationStrategyTest,
    testing::Values(
        MakeAdminVerificationStrategyTestParams{
            .testName = "NoPassword",
            .passwordOpt = std::nullopt,
            .expectIpStrategy = true,
            .expectPasswordStrategy = false
        },
        MakeAdminVerificationStrategyTestParams{
            .testName = "HasPassword",
            .passwordOpt = "p",
            .expectIpStrategy = false,
            .expectPasswordStrategy = true
        },
        MakeAdminVerificationStrategyTestParams{
            .testName = "EmptyPassword",
            .passwordOpt = "",
            .expectIpStrategy = false,
            .expectPasswordStrategy = true
        }
    )
);

struct MakeAdminVerificationStrategyFromConfigTestParams {
    std::string testName;
    std::string config;
    bool expectedError;
};

struct MakeAdminVerificationStrategyFromConfigTest
    : public testing::TestWithParam<MakeAdminVerificationStrategyFromConfigTestParams> {};

TEST_P(MakeAdminVerificationStrategyFromConfigTest, ChecksConfig)
{
    util::Config serverConfig{boost::json::parse(GetParam().config)};
    auto const result = web::impl::make_AdminVerificationStrategy(serverConfig);
    if (GetParam().expectedError) {
        EXPECT_FALSE(result.has_value());
    }
}

INSTANTIATE_TEST_SUITE_P(
    MakeAdminVerificationStrategyFromConfigTest,
    MakeAdminVerificationStrategyFromConfigTest,
    testing::Values(
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "NoPasswordNoLocalAdmin",
            .config = "{}",
            .expectedError = true
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "OnlyPassword",
            .config = R"({"admin_password": "password"})",
            .expectedError = false
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "OnlyLocalAdmin",
            .config = R"({"local_admin": true})",
            .expectedError = false
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "OnlyLocalAdminDisabled",
            .config = R"({"local_admin": false})",
            .expectedError = true
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "LocalAdminAndPassword",
            .config = R"({"local_admin": true, "admin_password": "password"})",
            .expectedError = true
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "LocalAdminDisabledAndPassword",
            .config = R"({"local_admin": false, "admin_password": "password"})",
            .expectedError = false
        }
    )
);
