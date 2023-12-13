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

#include <rpc/common/AnyHandler.h>
#include <rpc/handlers/AMMInfo.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <ripple/protocol/digest.h>
#include <fmt/core.h>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

constexpr static auto WRONG_AMM_ACCOUNT = "000S7XL6nxRAi7JcbJcn1Na179oF300000";
// constexpr static auto CORRECT_AMM_ACCOUNT = "rLcS7XL6nxRAi7JcbJcn1Na179oF3vdfbh";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";



class RPCAMMInfoHandlerTest : public HandlerBaseTest
{
};

struct AMMInfoParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AMMInfoParameterTest : public RPCAMMInfoHandlerTest,
                                  public WithParamInterface<AMMInfoParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
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
        AMMInfoParamTestCaseBundle{"MissingAMMAccountOrAssets", R"({})", "invalidParams", "Missing field 'amm_account' or both 'asset' and 'asset2'."},
        AMMInfoParamTestCaseBundle{"AMMAccountNotString", R"({"amm_account":1})", "invalidParams", "AMMAccountNotString"},
        AMMInfoParamTestCaseBundle{"AMMAccountInvalid", R"({"account":"xxx"})", "actMalformed", "accountMalformed"}
        //TODO: test for assetNotStringOrObject, assetInvalid, LPTokenAccountInvalid
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAMMInfoGroup1,
    AMMInfoParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    AMMInfoParameterTest::NameGenerator{});

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
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const static input = json::parse(fmt::format(
        R"({{
            "amm_account": "{}"
        }})",
        WRONG_AMM_ACCOUNT));
    auto const handler = AnyHandler{AMMInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}
