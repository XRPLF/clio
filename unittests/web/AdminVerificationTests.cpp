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

#include <util/Fixtures.h>

#include <web/impl/AdminVerificationStrategy.h>

#include <boost/json.hpp>
#include <gtest/gtest.h>

namespace http = boost::beast::http;

class IPAdminVerificationStrategyTest : public NoLoggerFixture {
protected:
    web::detail::IPAdminVerificationStrategy strat_;
    http::request<http::string_body> request_ = {};
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

    web::detail::PasswordAdminVerificationStrategy strat_{password_};

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
    MakeAdminVerificationStrategyTestParams(
        std::optional<std::string> passwordOpt,
        bool expectIpStrategy,
        bool expectPasswordStrategy
    )
        : passwordOpt(std::move(passwordOpt))
        , expectIpStrategy(expectIpStrategy)
        , expectPasswordStrategy(expectPasswordStrategy)
    {
    }
    std::optional<std::string> passwordOpt;
    bool expectIpStrategy;
    bool expectPasswordStrategy;
};

class MakeAdminVerificationStrategyTest : public testing::TestWithParam<MakeAdminVerificationStrategyTestParams> {};

TEST_P(MakeAdminVerificationStrategyTest, ChoosesStrategyCorrectly)
{
    auto strat = web::detail::make_AdminVerificationStrategy(GetParam().passwordOpt);
    auto ipStrat = dynamic_cast<web::detail::IPAdminVerificationStrategy*>(strat.get());
    EXPECT_EQ(ipStrat != nullptr, GetParam().expectIpStrategy);
    auto passwordStrat = dynamic_cast<web::detail::PasswordAdminVerificationStrategy*>(strat.get());
    EXPECT_EQ(passwordStrat != nullptr, GetParam().expectPasswordStrategy);
}

INSTANTIATE_TEST_CASE_P(
    MakeAdminVerificationStrategyTest,
    MakeAdminVerificationStrategyTest,
    testing::Values(
        MakeAdminVerificationStrategyTestParams(std::nullopt, true, false),
        MakeAdminVerificationStrategyTestParams("p", false, true),
        MakeAdminVerificationStrategyTestParams("", false, true)
    )
);
