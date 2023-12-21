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
constexpr static auto CORRECT_AMM_ACCOUNT = "rLcS7XL6nxRAi7JcbJcn1Na179oF3vdfbh";
constexpr static auto NOTFOUND_ACCOUNT = "rBdLS7RVLqkPwnWQCT2bC6HJd6xGoBizq8";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

class RPCAMMInfoHandlerTest : public HandlerBaseTest {};

struct AMMInfoParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AMMInfoParameterTest : public RPCAMMInfoHandlerTest, public WithParamInterface<AMMInfoParamTestCaseBundle> {
    struct NameGenerator {
        template <class ParamType>
        std::string
        operator()(testing::TestParamInfo<ParamType> const& info) const
        {
            auto bundle = static_cast<AMMInfoParamTestCaseBundle>(info.param);
            return bundle.testName;
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
        auto const handler = AnyHandler{AMMInfoHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotExist)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        WRONG_AMM_ACCOUNT
    ));
    auto const handler = AnyHandler{AMMInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotInDBIsMalformed)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        CORRECT_AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{mockBackendPtr}};

    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Amm account malformed.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AMMAccountNotFound)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    auto const accountRoot = CreateAccountRootObject(CORRECT_AMM_ACCOUNT, 0, 2, 200, 2, INDEX, 2);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(accountRoot.getSerializer().peekData()));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        CORRECT_AMM_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{mockBackendPtr}};

    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Amm account not found.");
    });
}

TEST_F(RPCAMMInfoHandlerTest, AccountNotFound)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    auto accountKey = GetAccountKey(NOTFOUND_ACCOUNT);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKey, testing::_, testing::_))
        .WillByDefault(Return(std::optional<Blob>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}",
            "account": "{}"
        }})",
        CORRECT_AMM_ACCOUNT,
        NOTFOUND_ACCOUNT
    ));

    auto const handler = AnyHandler{AMMInfoHandler{mockBackendPtr}};

    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}
