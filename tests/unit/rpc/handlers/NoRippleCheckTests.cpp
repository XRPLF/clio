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
#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/NoRippleCheck.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/TxFlags.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kTXN_ID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";

}  // namespace

struct RPCNoRippleCheckTest : HandlerBaseTest {
    RPCNoRippleCheckTest()
    {
        backend_->setRange(10, 30);
    }
};

struct NoRippleParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct NoRippleCheckParameterTest : RPCNoRippleCheckTest, WithParamInterface<NoRippleParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<NoRippleParamTestCaseBundle>{
        NoRippleParamTestCaseBundle{
            .testName = "AccountNotExists",
            .testJson = R"({
                "role": "gateway"
             })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'account' missing"
        },
        NoRippleParamTestCaseBundle{
            .testName = "AccountNotString",
            .testJson = R"({
                "account": 123,
                "role": "gateway"
             })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountNotString"
        },
        NoRippleParamTestCaseBundle{
            .testName = "InvalidAccount",
            .testJson = R"({
                "account": "123",
                "role": "gateway"
             })",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accountMalformed"
        },
        NoRippleParamTestCaseBundle{
            .testName = "InvalidRole",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "notrole"
             })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "role field is invalid"
        },
        NoRippleParamTestCaseBundle{
            .testName = "RoleNotExists",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
             })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'role' missing"
        },
        NoRippleParamTestCaseBundle{
            .testName = "LimitNotInt",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": "gg"
             })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NoRippleParamTestCaseBundle{
            .testName = "LimitNegative",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": -1
             })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NoRippleParamTestCaseBundle{
            .testName = "LimitZero",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": 0
             })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NoRippleParamTestCaseBundle{
            .testName = "TransactionsNotBool",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "transactions": "gg"
             })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCNoRippleCheckGroup1,
    NoRippleCheckParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(NoRippleCheckParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(NoRippleCheckParameterTest, V1ApiTransactionsIsNotBool)
{
    static constexpr auto kREQ_JSON = R"(
        {
            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "role": "gateway",
            "transactions": "gg"
         }
    )";

    EXPECT_CALL(*backend_, fetchLedgerBySequence);
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const req = json::parse(kREQ_JSON);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 1});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(std::nullopt));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "role": "gateway",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaIntIndex)
{
    constexpr auto kSEQ = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSEQ, _)).WillByDefault(Return(std::nullopt));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "role": "gateway",
            "ledger_index": {}
        }})",
        kACCOUNT,
        kSEQ
    ));
    auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaStringIndex)
{
    constexpr auto kSEQ = 12;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(kSEQ, _)).WillByDefault(Return(std::nullopt));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "role": "gateway",
            "ledger_index": "{}"
        }})",
        kACCOUNT,
        kSEQ
    ));
    auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, AccountNotExist)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return emtpy
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleUserDefaultRippleSetTrustLineNoRippleSet)
{
    static constexpr auto kSEQ = 30;
    static constexpr auto kEXPECTED_OUTPUT =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "problems":
            [
                "You appear to have set your default ripple flag even though you are not a gateway. This is not recommended unless you are experimenting"
            ],
            "validated":true
        })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(createAccountRootObject(kACCOUNT, ripple::lsfDefaultRipple, 2, 200, 2, kINDEX1, 2)
                                  .getSerializer()
                                  .peekData()));
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "user"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUTPUT));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleUserDefaultRippleUnsetTrustLineNoRippleUnSet)
{
    static constexpr auto kSEQ = 30;
    static constexpr auto kEXPECTED_OUTPUT =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "problems":[
                "You should probably set the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "You should probably set the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
            ],
            "validated":true
        })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2).getSerializer().peekData()));
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);

    auto const line2 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "user"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUTPUT));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleGatewayDefaultRippleSetTrustLineNoRippleSet)
{
    static constexpr auto kSEQ = 30;
    static constexpr auto kEXPECTED_OUTPUT =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "problems":
            [
                "You should clear the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "You should clear the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
            ],
            "validated":true
        })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(createAccountRootObject(kACCOUNT, ripple::lsfDefaultRipple, 2, 200, 2, kINDEX1, 2)
                                  .getSerializer()
                                  .peekData()));
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUTPUT));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleGatewayDefaultRippleUnsetTrustLineNoRippleUnset)
{
    static constexpr auto kSEQ = 30;
    static constexpr auto kEXPECTED_OUTPUT =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "problems":
            [
                "You should immediately set your default ripple flag"
            ],
            "validated":true
        })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2).getSerializer().peekData()));
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);

    auto const line2 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, 0);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUTPUT));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleGatewayDefaultRippleUnsetTrustLineNoRippleUnsetHighAccount)
{
    static constexpr auto kSEQ = 30;

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2).getSerializer().peekData()));
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillByDefault(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const line1 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT2, 10, kACCOUNT, 20, kTXN_ID, 123, 0);

    auto const line2 = createRippleStateLedgerObject("USD", kISSUER, 100, kACCOUNT2, 10, kACCOUNT, 20, kTXN_ID, 123, 0);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway",
            "transactions": true
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("transactions").as_array().size(), 1);
        EXPECT_EQ(output.result->as_object().at("problems").as_array().size(), 1);
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathLimit)
{
    constexpr auto kSEQ = 30;

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(createAccountRootObject(kACCOUNT, ripple::lsfDefaultRipple, 2, 200, 2, kINDEX1, 2)
                                  .getSerializer()
                                  .peekData()));
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway",
            "limit": 1
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("problems").as_array().size(), 1);
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathTransactions)
{
    constexpr auto kSEQ = 30;
    constexpr auto kTRANSACTION_SEQ = 123;
    auto const expectedOutput = fmt::format(
        R"({{
                "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index":30,
                "problems":[
                    "You should immediately set your default ripple flag",
                    "You should clear the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                    "You should clear the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
                ],
                "transactions":[
                    {{
                        "Sequence":{},
                        "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee":1,
                        "TransactionType":"AccountSet",
                        "SetFlag":8
                    }},
                    {{
                        "Sequence":{},
                        "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee":1,
                        "TransactionType":"TrustSet",
                        "LimitAmount":{{
                            "currency":"USD",
                            "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                            "value":"10"
                        }},
                        "Flags":{}
                    }},
                    {{
                        "Sequence":{},
                        "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee":1,
                        "TransactionType":"TrustSet",
                        "LimitAmount":{{
                            "currency":"USD",
                            "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                            "value":"10"
                        }},
                        "Flags":{}
                    }}
                ],
                "validated":true
        }})",
        kTRANSACTION_SEQ,
        kTRANSACTION_SEQ + 1,
        ripple::tfClearNoRipple,
        kTRANSACTION_SEQ + 2,
        ripple::tfClearNoRipple
    );

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(
            createAccountRootObject(kACCOUNT, 0, kTRANSACTION_SEQ, 200, 2, kINDEX1, 2).getSerializer().peekData()
        ));
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillByDefault(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    auto const line2 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway",
            "transactions": true
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, LimitMoreThanMax)
{
    constexpr auto kSEQ = 30;

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*backend_, doFetchLedgerObject)
        .WillByDefault(Return(createAccountRootObject(kACCOUNT, ripple::lsfDefaultRipple, 2, 200, 2, kINDEX1, 2)
                                  .getSerializer()
                                  .peekData()));
    auto const ownerDir = createOwnerDirLedgerObject(
        std::vector{NoRippleCheckHandler::kLIMIT_MAX + 1, ripple::uint256{kINDEX1}}, kINDEX1
    );
    auto const ownerDirKk = ripple::keylet::ownerDir(getAccountIdWithString(kACCOUNT)).key;
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, kSEQ, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const line1 = createRippleStateLedgerObject(
        "USD", kISSUER, 100, kACCOUNT, 10, kACCOUNT2, 20, kTXN_ID, 123, ripple::lsfLowNoRipple
    );

    std::vector<Blob> bbs;
    bbs.reserve(NoRippleCheckHandler::kLIMIT_MAX + 1);
    for (auto i = 0; i < NoRippleCheckHandler::kLIMIT_MAX + 1; i++) {
        bbs.push_back(line1.getSerializer().peekData());
    }

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway",
            "limit": {}
        }})",
        kACCOUNT,
        kLEDGER_HASH,
        NoRippleCheckHandler::kLIMIT_MAX + 1
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("problems").as_array().size(), NoRippleCheckHandler::kLIMIT_MAX);
    });
}
