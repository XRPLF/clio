#include "rpc/CredentialHelpers.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/Spawn.hpp"
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

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kIndex1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kCredentialId = "c7a14f6b9d5d4a9cb9c223a61b8e5c7df58e8b7ad1c6b4f8e7a321fa4e5b4c9d";
constexpr std::string_view kCredentialType = "credType";

}  // namespace

TEST(CreateAuthCredentialsTest, UniqueCredentials)
{
    xrpl::STArray credentials;
    auto const cred1 = createCredentialObject(kAccount, kAccount2, kCredentialType);
    auto const cred2 = createCredentialObject(kAccount2, kAccount, kCredentialType);

    credentials.push_back(cred1);
    credentials.push_back(cred2);

    auto const result = credentials::createAuthCredentials(credentials);

    // Validate that the result contains the correct set of credentials
    ASSERT_EQ(result.size(), 2);

    auto const cred1Type = cred1.getFieldVL(xrpl::sfCredentialType);
    auto const cred2Type = cred2.getFieldVL(xrpl::sfCredentialType);

    auto const expectedCred1 = std::make_pair(
        cred1.getAccountID(xrpl::sfIssuer), xrpl::Slice{cred1Type.data(), cred1Type.size()}
    );
    auto const expectedCred2 = std::make_pair(
        cred2.getAccountID(xrpl::sfIssuer), xrpl::Slice{cred2Type.data(), cred2Type.size()}
    );

    EXPECT_TRUE(result.count(expectedCred1));
    EXPECT_TRUE(result.count(expectedCred2));
}

TEST(ParseAuthorizeCredentialsTest, ValidCredentialsArray)
{
    boost::json::array credentials;
    boost::json::object credential1;
    credential1[JS(issuer)] = kAccount;
    credential1[JS(credential_type)] = xrpl::strHex(kCredentialType);

    credentials.push_back(credential1);
    xrpl::STArray const parsedCredentials = credentials::parseAuthorizeCredentials(credentials);

    ASSERT_EQ(parsedCredentials.size(), 1);

    xrpl::STObject const& cred = parsedCredentials[0];
    ASSERT_TRUE(cred.isFieldPresent(xrpl::sfIssuer));
    ASSERT_TRUE(cred.isFieldPresent(xrpl::sfCredentialType));

    auto const expectedIssuer =
        *xrpl::parseBase58<xrpl::AccountID>(  // NOLINT(bugprone-unchecked-optional-access)
            static_cast<std::string>(credential1[JS(issuer)].as_string())
        );
    auto const expectedCredentialType =
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        *xrpl::strUnHex(static_cast<std::string>(credential1[JS(credential_type)].as_string()));

    EXPECT_EQ(cred.getAccountID(xrpl::sfIssuer), expectedIssuer);
    EXPECT_EQ(cred.getFieldVL(xrpl::sfCredentialType), expectedCredentialType);
}

class CredentialHelperTest : public util::prometheus::WithPrometheus,
                             public MockBackendTest,
                             public SyncAsioContextTest {};

TEST_F(CredentialHelperTest, GetInvalidCredentialArray)
{
    boost::json::array credentialsArray = {kCredentialId};
    auto const info = createLedgerHeader(kIndex1, 30);

    util::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto const ret = credentials::fetchCredentialArray(
            credentialsArray, getAccountIdWithString(kAccount), *backend_, info, yield
        );
        ASSERT_FALSE(ret.has_value());
        auto const status = ret.error();
        EXPECT_EQ(status, RippledError::RpcBadCredentials);
        EXPECT_EQ(status.message, "credentials don't exist.");
    });
    ctx_.run();
}

TEST_F(CredentialHelperTest, GetValidCredentialArray)
{
    backend_->setRange(10, 30);

    auto ledgerHeader = createLedgerHeader(kIndex1, 30);
    auto const credLedgerObject =
        createCredentialObject(kAccount, kAccount2, kCredentialType, true);

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _))
        .WillByDefault(Return(credLedgerObject.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    boost::json::array credentialsArray = {kCredentialId};

    xrpl::STArray expectedAuthCreds;
    xrpl::STObject credential(xrpl::sfCredential);
    credential.setAccountID(xrpl::sfIssuer, getAccountIdWithString(kAccount2));
    credential.setFieldVL(
        xrpl::sfCredentialType, xrpl::Blob{std::begin(kCredentialType), std::end(kCredentialType)}
    );
    expectedAuthCreds.push_back(std::move(credential));

    util::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto const result = credentials::fetchCredentialArray(
            credentialsArray, getAccountIdWithString(kAccount), *backend_, ledgerHeader, yield
        );
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), expectedAuthCreds);
    });
    ctx_.run();
}
