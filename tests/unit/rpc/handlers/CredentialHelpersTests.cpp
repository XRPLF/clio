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

#include "rpc/CredentialHelpers.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/jss.h>

#include <string>
#include <string_view>
#include <utility>

using namespace rpc;
using namespace testing;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kINDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kCREDENTIAL_ID = "c7a14f6b9d5d4a9cb9c223a61b8e5c7df58e8b7ad1c6b4f8e7a321fa4e5b4c9d";
constexpr std::string_view kCREDENTIAL_TYPE = "credType";

}  // namespace

TEST(CreateAuthCredentialsTest, UniqueCredentials)
{
    ripple::STArray credentials;
    auto const cred1 = createCredentialObject(kACCOUNT, kACCOUNT2, kCREDENTIAL_TYPE);
    auto const cred2 = createCredentialObject(kACCOUNT2, kACCOUNT, kCREDENTIAL_TYPE);

    credentials.push_back(cred1);
    credentials.push_back(cred2);

    auto const result = credentials::createAuthCredentials(credentials);

    // Validate that the result contains the correct set of credentials
    ASSERT_EQ(result.size(), 2);

    auto const cred1Type = cred1.getFieldVL(ripple::sfCredentialType);
    auto const cred2Type = cred2.getFieldVL(ripple::sfCredentialType);

    auto const expectedCred1 =
        std::make_pair(cred1.getAccountID(ripple::sfIssuer), ripple::Slice{cred1Type.data(), cred1Type.size()});
    auto const expectedCred2 =
        std::make_pair(cred2.getAccountID(ripple::sfIssuer), ripple::Slice{cred2Type.data(), cred2Type.size()});

    EXPECT_TRUE(result.count(expectedCred1));
    EXPECT_TRUE(result.count(expectedCred2));
}

TEST(ParseAuthorizeCredentialsTest, ValidCredentialsArray)
{
    boost::json::array credentials;
    boost::json::object credential1;
    credential1[JS(issuer)] = kACCOUNT;
    credential1[JS(credential_type)] = ripple::strHex(kCREDENTIAL_TYPE);

    credentials.push_back(credential1);
    ripple::STArray const parsedCredentials = credentials::parseAuthorizeCredentials(credentials);

    ASSERT_EQ(parsedCredentials.size(), 1);

    ripple::STObject const& cred = parsedCredentials[0];
    ASSERT_TRUE(cred.isFieldPresent(ripple::sfIssuer));
    ASSERT_TRUE(cred.isFieldPresent(ripple::sfCredentialType));

    auto const expectedIssuer =
        *ripple::parseBase58<ripple::AccountID>(static_cast<std::string>(credential1[JS(issuer)].as_string()));
    auto const expectedCredentialType =
        ripple::strUnHex(static_cast<std::string>(credential1[JS(credential_type)].as_string())).value();

    EXPECT_EQ(cred.getAccountID(ripple::sfIssuer), expectedIssuer);
    EXPECT_EQ(cred.getFieldVL(ripple::sfCredentialType), expectedCredentialType);
}

class CredentialHelperTest : public util::prometheus::WithPrometheus,
                             public MockBackendTest,
                             public SyncAsioContextTest {};

TEST_F(CredentialHelperTest, GetInvalidCredentialArray)
{
    boost::json::array credentialsArray = {kCREDENTIAL_ID};
    auto const info = createLedgerHeader(kINDEX1, 30);

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto const ret = credentials::fetchCredentialArray(
            credentialsArray, getAccountIdWithString(kACCOUNT), *backend_, info, yield
        );
        ASSERT_FALSE(ret.has_value());
        auto const status = ret.error();
        EXPECT_EQ(status, RippledError::rpcBAD_CREDENTIALS);
        EXPECT_EQ(status.message, "credentials don't exist.");
    });
    ctx_.run();
}

TEST_F(CredentialHelperTest, GetValidCredentialArray)
{
    backend_->setRange(10, 30);

    auto ledgerHeader = createLedgerHeader(kINDEX1, 30);
    auto const credLedgerObject = createCredentialObject(kACCOUNT, kACCOUNT2, kCREDENTIAL_TYPE, true);

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(credLedgerObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    boost::json::array credentialsArray = {kCREDENTIAL_ID};

    ripple::STArray expectedAuthCreds;
    ripple::STObject credential(ripple::sfCredential);
    credential.setAccountID(ripple::sfIssuer, getAccountIdWithString(kACCOUNT2));
    credential.setFieldVL(
        ripple::sfCredentialType, ripple::Blob{std::begin(kCREDENTIAL_TYPE), std::end(kCREDENTIAL_TYPE)}
    );
    expectedAuthCreds.push_back(std::move(credential));

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto const result = credentials::fetchCredentialArray(
            credentialsArray, getAccountIdWithString(kACCOUNT), *backend_, ledgerHeader, yield
        );
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), expectedAuthCreds);
    });
    ctx_.run();
}
