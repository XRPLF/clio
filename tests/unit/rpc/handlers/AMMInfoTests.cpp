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

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/AMMInfo.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/UintTypes.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kSEQ = 30;
constexpr auto kWRONG_AMM_ACCOUNT = "000S7XL6nxRAi7JcbJcn1Na179oF300000";
constexpr auto kAMM_ACCOUNT = "rLcS7XL6nxRAi7JcbJcn1Na179oF3vdfbh";
constexpr auto kAMM_ACCOUNT2 = "rnW8FAPgpQgA6VoESnVrUVJHBdq9QAtRZs";
constexpr auto kLP_ISSUE_CURRENCY = "03930D02208264E2E40EC1B0C09E4DB96EE197B1";
constexpr auto kNOTFOUND_ACCOUNT = "rBdLS7RVLqkPwnWQCT2bC6HJd6xGoBizq8";
constexpr auto kAMM_ID = 54321;
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

}  // namespace

struct RPCAMMInfoHandlerTest : HandlerBaseTest {
    RPCAMMInfoHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

struct AMMInfoParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

struct AMMInfoParameterTest : RPCAMMInfoHandlerTest, WithParamInterface<AMMInfoParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AMMInfoParamTestCaseBundle>{
        AMMInfoParamTestCaseBundle{
            .testName = "MissingAMMAccountOrAssets",
            .testJson = "{}",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAccountNotString",
            .testJson = R"({"amm_account": 1})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "Account malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AccountNotString",
            .testJson = R"({"account": 1})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "Account malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAccountInvalid",
            .testJson = R"({"amm_account": "xxx"})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "Account malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AccountInvalid",
            .testJson = R"({"account": "xxx"})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "Account malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAssetNotStringOrObject",
            .testJson = R"({"asset": 1})",
            .expectedError = "issueMalformed",
            .expectedErrorMessage = "Issue is malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAssetEmptyObject",
            .testJson = R"({"asset": {}})",
            .expectedError = "issueMalformed",
            .expectedErrorMessage = "Issue is malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAsset2NotStringOrObject",
            .testJson = R"({"asset2": 1})",
            .expectedError = "issueMalformed",
            .expectedErrorMessage = "Issue is malformed."
        },
        AMMInfoParamTestCaseBundle{
            .testName = "AMMAsset2EmptyObject",
            .testJson = R"({"asset2": {}})",
            .expectedError = "issueMalformed",
            .expectedErrorMessage = "Issue is malformed."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAMMInfoGroup1,
    AMMInfoParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(AMMInfoParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AMMInfoHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAMMInfoHandlerTest, AccountNotFound)
{
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, 30);
    auto const missingAccountKey = getAccountKey(kNOTFOUND_ACCOUNT);
    auto const accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const accountKey = getAccountKey(kAMM_ACCOUNT);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(missingAccountKey, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));
    ON_CALL(*backend_, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}",
            "account": "{}"
        }})",
        kAMM_ACCOUNT,
        kNOTFOUND_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotExist)
{
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kWRONG_AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotInDBIsMalformed)
{
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotFoundMissingAmmField)
{
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, 30);
    auto const accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(accountRoot.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountAmmBlobNotFound)
{
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, 30);
    auto const accountKey = getAccountKey(kAMM_ACCOUNT);
    auto const ammId = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammId);

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(kAMM_ACCOUNT2, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", kAMM_ACCOUNT2);
    accountRoot.setFieldH256(ripple::sfAMMID, ripple::uint256{kAMM_ID});

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountAccBlobNotFound)
{
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, 30);
    auto const accountKey = getAccountKey(kAMM_ACCOUNT);
    auto const account2Key = getAccountKey(kAMM_ACCOUNT2);
    auto const ammId = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammId);

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const ammObj =
        createAmmObject(kAMM_ACCOUNT2, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", kAMM_ACCOUNT2);
    accountRoot.setFieldH256(ripple::sfAMMID, ammId);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(account2Key, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathMinimalFirstXRPNoTrustline)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue2LineKey = ripple::keylet::line(account1, account2, ripple::to_currency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(
        kAMM_ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY
    );
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _)).WillByDefault(Return(std::optional<Blob>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": "193",
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "asset2_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithAccount)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue2LineKey = ripple::keylet::line(account2, account1, ripple::to_currency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const account2Root = createAccountRootObject(kAMM_ACCOUNT2, 0, 2, 300, 2, kINDEX1, 2);
    auto const ammObj = createAmmObject(
        kAMM_ACCOUNT2, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", kAMM_ACCOUNT, kLP_ISSUE_CURRENCY
    );
    auto const lptCurrency = createLptCurrency("XRP", "JPY");
    auto const accountHoldsKeylet = ripple::keylet::line(account2, account2, lptCurrency);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    auto const trustline = createRippleStateLedgerObject(
        kLP_ISSUE_CURRENCY, kAMM_ACCOUNT, 12, kAMM_ACCOUNT2, 1000, kAMM_ACCOUNT, 2000, kINDEX1, 2
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _)).WillByDefault(Return(std::optional<Blob>{}));
    ON_CALL(*backend_, doFetchLedgerObject(accountHoldsKeylet.key, kSEQ, _))
        .WillByDefault(Return(trustline.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}",
            "account": "{}"
        }})",
        kAMM_ACCOUNT,
        kAMM_ACCOUNT2
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto const expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "12"
                    }},
                    "amount": "293",
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "asset2_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT2,
            "JPY",
            kAMM_ACCOUNT,
            kAMM_ACCOUNT2,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathMinimalSecondXRPNoTrustline)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue2LineKey = ripple::keylet::line(account1, account2, ripple::to_currency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(
        kAMM_ACCOUNT, "JPY", kAMM_ACCOUNT2, "XRP", ripple::toBase58(ripple::xrpAccount()), kLP_ISSUE_CURRENCY
    );
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _)).WillByDefault(Return(std::optional<Blob>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto const expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "amount2": "193",
                    "account": "{}",
                    "trading_fee": 5,
                    "asset_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathNonXRPNoTrustlines)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue2LineKey = ripple::keylet::line(account1, account2, ripple::to_currency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(kAMM_ACCOUNT, "USD", kAMM_ACCOUNT, "JPY", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY);
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _)).WillByDefault(Return(std::optional<Blob>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto const expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "asset_frozen": false,
                    "asset2_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "USD",
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathFrozen)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue1LineKey = ripple::keylet::line(account1, account1, ripple::to_currency("USD")).key;
    auto const issue2LineKey = ripple::keylet::line(account1, account2, ripple::to_currency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(kAMM_ACCOUNT, "USD", kAMM_ACCOUNT, "JPY", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY);
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    // note: frozen flag will not be used for trustline1 because issuer == account
    auto const trustline1BalanceFrozen = createRippleStateLedgerObject(
        "USD", kAMM_ACCOUNT, 8, kAMM_ACCOUNT, 1000, kAMM_ACCOUNT2, 2000, kINDEX1, 2, ripple::lsfGlobalFreeze
    );
    auto const trustline2BalanceFrozen = createRippleStateLedgerObject(
        "JPY", kAMM_ACCOUNT, 12, kAMM_ACCOUNT2, 1000, kAMM_ACCOUNT, 2000, kINDEX1, 2, ripple::lsfGlobalFreeze
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue1LineKey, kSEQ, _))
        .WillByDefault(Return(trustline1BalanceFrozen.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _))
        .WillByDefault(Return(trustline2BalanceFrozen.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto const expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "8"
                    }},
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "-12"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "asset_frozen": false,
                    "asset2_frozen": true
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "USD",
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathFrozenIssuer)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue1LineKey = ripple::keylet::line(account1, account1, ripple::to_currency("USD")).key;
    auto const issue2LineKey = ripple::keylet::line(account1, account2, ripple::to_currency("JPY")).key;

    // asset1 will be frozen because flag set here
    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, ripple::lsfGlobalFreeze, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(kAMM_ACCOUNT, "USD", kAMM_ACCOUNT, "JPY", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY);
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    // note: frozen flag will not be used for trustline1 because issuer == account
    auto const trustline1BalanceFrozen = createRippleStateLedgerObject(
        "USD", kAMM_ACCOUNT, 8, kAMM_ACCOUNT, 1000, kAMM_ACCOUNT2, 2000, kINDEX1, 2, ripple::lsfGlobalFreeze
    );
    auto const trustline2BalanceFrozen = createRippleStateLedgerObject(
        "JPY", kAMM_ACCOUNT, 12, kAMM_ACCOUNT2, 1000, kAMM_ACCOUNT, 2000, kINDEX1, 2, ripple::lsfGlobalFreeze
    );

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue1LineKey, kSEQ, _))
        .WillByDefault(Return(trustline1BalanceFrozen.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _))
        .WillByDefault(Return(trustline2BalanceFrozen.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto const expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "8"
                    }},
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "-12"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "asset_frozen": true,
                    "asset2_frozen": true
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "USD",
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithTrustline)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue2LineKey = ripple::keylet::line(account1, account2, ripple::to_currency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(
        kAMM_ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY
    );
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    auto const trustlineBalance =
        createRippleStateLedgerObject("JPY", kAMM_ACCOUNT2, -8, kAMM_ACCOUNT, 1000, kAMM_ACCOUNT2, 2000, kINDEX2, 2, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _))
        .WillByDefault(Return(trustlineBalance.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": "193",
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "8"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "asset2_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithVoteSlots)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue2LineKey = ripple::keylet::line(account1, account2, ripple::to_currency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(
        kAMM_ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY
    );
    ammAddVoteSlot(ammObj, account1, 2, 4);
    ammAddVoteSlot(ammObj, account2, 4, 2);
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    auto const trustlineBalance =
        createRippleStateLedgerObject("JPY", kAMM_ACCOUNT2, -8, kAMM_ACCOUNT, 1000, kAMM_ACCOUNT2, 2000, kINDEX2, 2, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _))
        .WillByDefault(Return(trustlineBalance.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": "193",
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "8"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "vote_slots": [
                        {{
                            "account": "{}",
                            "trading_fee": 2,
                            "vote_weight": 4
                        }},
                        {{
                            "account": "{}",
                            "trading_fee": 4,
                            "vote_weight": 2
                        }}
                    ],
                    "asset2_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kAMM_ACCOUNT,
            kAMM_ACCOUNT2,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithAuctionSlot)
{
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const ammKey = ripple::uint256{kAMM_ID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);
    auto const feesKey = ripple::keylet::fees().key;
    auto const issue2LineKey = ripple::keylet::line(account1, account2, ripple::to_currency("JPY")).key;

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(
        kAMM_ACCOUNT, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY
    );
    ammSetAuctionSlot(
        ammObj, account2, ripple::amountFromString(ripple::xrpIssue(), "100"), 2, 25 * 3600, {account1, account2}
    );

    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);
    auto const feesObj = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    auto const trustlineBalance =
        createRippleStateLedgerObject("JPY", kAMM_ACCOUNT2, -8, kAMM_ACCOUNT, 1000, kAMM_ACCOUNT2, 2000, kINDEX2, 2, 0);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(feesKey, kSEQ, _)).WillByDefault(Return(feesObj));
    ON_CALL(*backend_, doFetchLedgerObject(issue2LineKey, kSEQ, _))
        .WillByDefault(Return(trustlineBalance.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        kAMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": "193",
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "8"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "auction_slot": {{
                        "time_interval": 20,
                        "price": "100",
                        "discounted_fee": 2,
                        "account": "{}",
                        "expiration": "2000-01-02T01:00:00+0000",
                        "auth_accounts": [
                            {{
                                "account": "{}"
                            }},
                            {{
                                "account": "{}"
                            }}
                        ]
                    }},
                    "asset2_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kAMM_ACCOUNT2,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithAssetsMatchingInputOrder)
{
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const issue1 = ripple::Issue(ripple::to_currency("JPY"), account1);
    auto const issue2 = ripple::Issue(ripple::to_currency("USD"), account2);
    auto const ammKeylet = ripple::keylet::amm(issue1, issue2);

    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto ammObj = createAmmObject(kAMM_ACCOUNT, "JPY", kAMM_ACCOUNT, "USD", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY);
    auto const auctionIssue = ripple::Issue{ripple::Currency{kLP_ISSUE_CURRENCY}, account1};
    ammSetAuctionSlot(
        ammObj, account2, ripple::amountFromString(auctionIssue, "100"), 2, 25 * 3600, {account1, account2}
    );
    accountRoot.setFieldH256(ripple::sfAMMID, ammKeylet.key);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "asset": {{
                "currency": "JPY", 
                "issuer": "{}"
            }},
            "asset2": {{
                "currency": "USD",
                "issuer": "{}"
            }}
        }})",
        kAMM_ACCOUNT,
        kAMM_ACCOUNT2
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "auction_slot": {{
                        "time_interval": 20,
                        "price": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "discounted_fee": 2,
                        "account": "{}",
                        "expiration": "2000-01-02T01:00:00+0000",
                        "auth_accounts": [
                            {{
                                "account": "{}"
                            }},
                            {{
                                "account": "{}"
                            }}
                        ]
                    }},
                    "asset_frozen": false,
                    "asset2_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT,
            "USD",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kAMM_ACCOUNT2,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPathWithAssetsPreservesInputOrder)
{
    auto const lgrInfo = createLedgerHeader(kLEDGER_HASH, kSEQ);
    auto const account1 = getAccountIdWithString(kAMM_ACCOUNT);
    auto const account2 = getAccountIdWithString(kAMM_ACCOUNT2);
    auto const issue1 = ripple::Issue(ripple::to_currency("USD"), account1);
    auto const issue2 = ripple::Issue(ripple::to_currency("JPY"), account2);
    auto const ammKeylet = ripple::keylet::amm(issue1, issue2);

    // Note: order in the AMM object is different from the input
    auto ammObj = createAmmObject(kAMM_ACCOUNT, "JPY", kAMM_ACCOUNT, "USD", kAMM_ACCOUNT2, kLP_ISSUE_CURRENCY);
    auto accountRoot = createAccountRootObject(kAMM_ACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const auctionIssue = ripple::Issue{ripple::Currency{kLP_ISSUE_CURRENCY}, account1};
    ammSetAuctionSlot(
        ammObj, account2, ripple::amountFromString(auctionIssue, "100"), 2, 25 * 3600, {account1, account2}
    );
    accountRoot.setFieldH256(ripple::sfAMMID, ammKeylet.key);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account1), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(getAccountKey(account2), testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "asset": {{
                "currency": "USD",
                "issuer": "{}"
            }},
            "asset2": {{
                "currency": "JPY", 
                "issuer": "{}"
            }}
        }})",
        kAMM_ACCOUNT,
        kAMM_ACCOUNT2
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        auto expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "amount2": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "0"
                    }},
                    "account": "{}",
                    "trading_fee": 5,
                    "auction_slot": {{
                        "time_interval": 20,
                        "price": {{
                            "currency": "{}",
                            "issuer": "{}",
                            "value": "100"
                        }},
                        "discounted_fee": 2,
                        "account": "{}",
                        "expiration": "2000-01-02T01:00:00+0000",
                        "auth_accounts": [
                            {{
                                "account": "{}"
                            }},
                            {{
                                "account": "{}"
                            }}
                        ]
                    }},
                    "asset_frozen": false,
                    "asset2_frozen": false
                }},
                "ledger_index": 30,
                "ledger_hash": "{}",
                "validated": true
            }})",
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            "USD",
            kAMM_ACCOUNT,
            "JPY",
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kLP_ISSUE_CURRENCY,
            kAMM_ACCOUNT,
            kAMM_ACCOUNT2,
            kAMM_ACCOUNT,
            kAMM_ACCOUNT2,
            kLEDGER_HASH
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expectedResult);
    });
}
