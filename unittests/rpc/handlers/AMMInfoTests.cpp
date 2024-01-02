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

#include "rpc/common/AnyHandler.h"
#include "rpc/handlers/AMMInfo.h"
#include "util/Fixtures.h"
#include "util/TestObject.h"

#include <fmt/core.h>
#include <ripple/protocol/digest.h>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

constexpr static auto WRONG_AMM_ACCOUNT = "000S7XL6nxRAi7JcbJcn1Na179oF300000";
constexpr static auto AMM_ACCOUNT = "rLcS7XL6nxRAi7JcbJcn1Na179oF3vdfbh";
constexpr static auto AMM_ACCOUNT2 = "rnW8FAPgpQgA6VoESnVrUVJHBdq9QAtRZs";
constexpr static auto NOTFOUND_ACCOUNT = "rBdLS7RVLqkPwnWQCT2bC6HJd6xGoBizq8";
constexpr static auto AMMID = 54321;
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

// TODO: change back to HandlerBaseTest when done writing tests
class RPCAMMInfoHandlerTest : public HandlerBaseTestNaggy {};

struct AMMInfoParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AMMInfoParameterTest : public RPCAMMInfoHandlerTest, public WithParamInterface<AMMInfoParamTestCaseBundle> {
    struct NameGenerator {
        std::string
        operator()(auto const& info) const
        {
            return static_cast<AMMInfoParamTestCaseBundle>(info.param).testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AMMInfoParamTestCaseBundle>{
        AMMInfoParamTestCaseBundle{"MissingAMMAccountOrAssets", "{}", "invalidParams", "Invalid parameters."},
        AMMInfoParamTestCaseBundle{
            "AMMAccountNotString", R"({"amm_account": 1})", "actMalformed", "Account malformed."
        },
        AMMInfoParamTestCaseBundle{"AccountNotString", R"({"account": 1})", "actMalformed", "Account malformed."},
        AMMInfoParamTestCaseBundle{
            "AMMAccountInvalid", R"({"amm_account": "xxx"})", "actMalformed", "Account malformed."
        },
        AMMInfoParamTestCaseBundle{"AccountInvalid", R"({"account": "xxx"})", "actMalformed", "Account malformed."},
        AMMInfoParamTestCaseBundle{
            "AMMAssetNotStringOrObject", R"({"asset": 1})", "issueMalformed", "Issue is malformed."
        },
        AMMInfoParamTestCaseBundle{"AMMAssetEmptyObject", R"({"asset": {}})", "issueMalformed", "Issue is malformed."},
        AMMInfoParamTestCaseBundle{
            "AMMAsset2NotStringOrObject", R"({"asset2": 1})", "issueMalformed", "Issue is malformed."
        },
        AMMInfoParamTestCaseBundle{
            "AMMAsset2EmptyObject", R"({"asset2": {}})", "issueMalformed", "Issue is malformed."
        },
        // TODO: LPTokenAccountInvalid??
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAMMInfoGroup1,
    AMMInfoParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    AMMInfoParameterTest::NameGenerator{}
);

TEST_P(AMMInfoParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AMMInfoHandler{backend}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAMMInfoHandlerTest, AccountNotFound)
{
    backend->setRange(10, 30);

    auto const lgrInfo = CreateLedgerInfo(LEDGERHASH, 30);
    auto const accountKey = GetAccountKey(NOTFOUND_ACCOUNT);

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}",
            "account": "{}"
        }})",
        AMM_ACCOUNT,
        NOTFOUND_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotExist)
{
    backend->setRange(10, 30);

    auto const lgrInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        WRONG_AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(
            err.at("error_message").as_string(), "Account malformed."
        );  // TODO: is this supposed to say Amm account malformed?
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotInDBIsMalformed)
{
    backend->setRange(10, 30);

    auto const lgrInfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Amm account malformed.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotFoundMissingAmmField)
{
    backend->setRange(10, 30);

    auto const lgrInfo = CreateLedgerInfo(LEDGERHASH, 30);
    auto const accountRoot = CreateAccountRootObject(AMM_ACCOUNT, 0, 2, 200, 2, INDEX, 2);

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(accountRoot.getSerializer().peekData()));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Amm account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountAmmBlobNotFound)
{
    backend->setRange(10, 30);

    auto const lgrInfo = CreateLedgerInfo(LEDGERHASH, 30);
    auto const accountKey = GetAccountKey(AMM_ACCOUNT);
    auto const account2Key = GetAccountKey(AMM_ACCOUNT2);

    auto accountRoot = CreateAccountRootObject(AMM_ACCOUNT, 0, 2, 200, 2, INDEX, 2);
    auto ammObj = CreateAMMObject(AMM_ACCOUNT2, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", AMM_ACCOUNT2);
    accountRoot.setFieldH256(ripple::sfAMMID, ripple::uint256{AMMID});

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(account2Key, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Amm account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountAccBlobNotFound)
{
    backend->setRange(10, 30);

    auto const lgrInfo = CreateLedgerInfo(LEDGERHASH, 30);
    auto const accountKey = GetAccountKey(AMM_ACCOUNT);
    auto const account2Key = GetAccountKey(AMM_ACCOUNT2);
    auto const ammKey = ripple::uint256{AMMID};

    auto accountRoot = CreateAccountRootObject(AMM_ACCOUNT, 0, 2, 200, 2, INDEX, 2);
    auto ammObj = CreateAMMObject(AMM_ACCOUNT2, "XRP", ripple::toBase58(ripple::xrpAccount()), "JPY", AMM_ACCOUNT2);
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(account2Key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ammKey, testing::_, testing::_)).WillByDefault(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Amm account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, HappyPath1)
{
    backend->setRange(10, 30);

    auto const lgrInfo = CreateLedgerInfo(LEDGERHASH, 30);
    auto const accountKey = GetAccountKey(AMM_ACCOUNT);
    auto const account2Key = GetAccountKey(AMM_ACCOUNT2);
    auto const ammKey = ripple::uint256{AMMID};
    auto const ammKeylet = ripple::keylet::amm(ammKey);

    auto accountRoot = CreateAccountRootObject(AMM_ACCOUNT, 0, 2, 200, 2, INDEX, 2);
    auto ammObj = CreateAMMObject(
        AMM_ACCOUNT,
        "XRP",
        ripple::toBase58(ripple::xrpAccount()),
        "JPY",
        AMM_ACCOUNT2,
        "03930D02208264E2E40EC1B0C09E4DB96EE197B1"
    );
    accountRoot.setFieldH256(ripple::sfAMMID, ammKey);

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(lgrInfo));
    ON_CALL(*backend, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ammKeylet.key, testing::_, testing::_))
        .WillByDefault(Return(ammObj.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(account2Key, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        auto expectedResult = json::parse(fmt::format(
            R"({{
                "amm": {{
                    "lp_token": {{
                        "currency": "{}",
                        "issuer": "{}",
                        "value": "100"
                    }},
                    "amount": "0",
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
                "validated": true
            }})",
            "03930D02208264E2E40EC1B0C09E4DB96EE197B1",
            AMM_ACCOUNT,
            "JPY",
            AMM_ACCOUNT2,
            AMM_ACCOUNT
        ));

        ASSERT_TRUE(output);
        EXPECT_EQ(output.value(), expectedResult);
    });
}
