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
#include "rpc/handlers/DepositAuthorized.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>

#include <chrono>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kINDEX2 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1";
constexpr auto kCREDENTIAL_HASH = "F245428267E6177AEEFDD4FEA3533285712A4B1091CF82A7EA7BC39A62C3FB1A";
constexpr std::string_view kCREDENTIAL_TYPE = "credType";

constexpr auto kRANGE_MIN = 10;
constexpr auto kRANGE_MAX = 30;

}  // namespace

using namespace rpc;
namespace json = boost::json;
using namespace testing;

struct RPCDepositAuthorizedTest : HandlerBaseTest {
    RPCDepositAuthorizedTest()
    {
        backend_->setRange(kRANGE_MIN, kRANGE_MAX);
    }
};

struct DepositAuthorizedTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct DepositAuthorizedParameterTest : RPCDepositAuthorizedTest,
                                        WithParamInterface<DepositAuthorizedTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<DepositAuthorizedTestCaseBundle>{
        {
            .testName = "SourceAccountMissing",
            .testJson = R"({
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'source_account' missing",
        },
        {
            .testName = "SourceAccountMalformed",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "source_accountMalformed",
        },
        {
            .testName = "SourceAccountNotString",
            .testJson = R"({
                "source_account": 1234, 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "source_accountNotString",
        },
        {
            .testName = "DestinationAccountMissing",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'destination_account' missing",
        },
        {
            .testName = "DestinationAccountMalformed",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "destination_accountMalformed",
        },
        {
            .testName = "DestinationAccountNotString",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": 1234,
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "destination_accountNotString",
        },
        {
            .testName = "LedgerHashInvalid",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "x"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed",
        },
        {
            .testName = "LedgerHashNotString",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString",
        },
        {
            .testName = "LedgerIndexNotInt",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_index": "x"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed",
        },
        {
            .testName = "CredentialsNotArray",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "credentials": "x"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters.",
        },
        {
            .testName = "CredentialsNotStringsInArray",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "credentials": [123]
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Item is not a valid uint256 type.",
        },
        {
            .testName = "CredentialsNotHexedStringInArray",
            .testJson = R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "credentials": ["234", "432"]
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Item is not a valid uint256 type.",
        }
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCDepositAuthorizedGroup,
    DepositAuthorizedParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(DepositAuthorizedParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_index": {}
            }})",
            kACCOUNT,
            kACCOUNT2,
            kRANGE_MAX
        ));

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaStringSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_index": "{}"
            }})",
            kACCOUNT,
            kACCOUNT2,
            kRANGE_MAX
        ));

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_hash": "{}"
            }})",
            kACCOUNT,
            kACCOUNT2,
            kLEDGER_HASH
        ));

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, SourceAccountDoesNotExist)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "srcActNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "source_accountNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, DestinationAccountDoesNotExist)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const accountRoot = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(std::optional<Blob>{}));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "dstActNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "destination_accountNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, AccountsAreEqual)
{
    static constexpr auto kEXPECTED_OUT =
        R"({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
        })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const accountRoot = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(accountRoot.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kACCOUNT,
        kLEDGER_HASH
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsNoDepositAuthFlag)
{
    static constexpr auto kEXPECTED_OUT =
        R"({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, 0, 2, 200, 2, kINDEX2, 2);

    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsWithDepositAuthFlagReturnsFalse)
{
    static constexpr auto kEXPECTED_OUT =
        R"({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": false,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::nullopt));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsWithDepositAuthFlagReturnsTrue)
{
    static constexpr auto kEXPECTED_OUT =
        R"({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialAcceptedAndNotExpiredReturnsTrue)
{
    static auto const kEXPECTED_OUT = fmt::format(
        R"({{
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "credentials": ["{}"]
        }})",
        kCREDENTIAL_HASH  // CREDENTIALHASH should match credentialIndex
    );

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);
    auto const credential = createCredentialObject(kACCOUNT, kACCOUNT2, kCREDENTIAL_TYPE);
    auto const credentialIndex = ripple::keylet::credential(
                                     getAccountIdWithString(kACCOUNT),
                                     getAccountIdWithString(kACCOUNT2),
                                     ripple::Slice(kCREDENTIAL_TYPE.data(), kCREDENTIAL_TYPE.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(4);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}"]
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH,
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(kEXPECTED_OUT));
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialNotAuthorizedReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);
    auto const credential = createCredentialObject(kACCOUNT, kACCOUNT2, kCREDENTIAL_TYPE, false);
    auto const credentialIndex = ripple::keylet::credential(
                                     getAccountIdWithString(kACCOUNT),
                                     getAccountIdWithString(kACCOUNT2),
                                     ripple::Slice(kCREDENTIAL_TYPE.data(), kCREDENTIAL_TYPE.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}"]
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH,
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "credentials aren't accepted");
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialExpiredReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30, 100);

    // set parent close time to 500 seconds
    ledgerHeader.parentCloseTime = ripple::NetClock::time_point{std::chrono::seconds{500}};

    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);

    // credential expire time is 23 seconds, so credential will fail
    auto const expiredCredential = createCredentialObject(kACCOUNT, kACCOUNT2, kCREDENTIAL_TYPE, true, 23);

    auto const credentialIndex = ripple::keylet::credential(
                                     getAccountIdWithString(kACCOUNT),
                                     getAccountIdWithString(kACCOUNT2),
                                     ripple::Slice(kCREDENTIAL_TYPE.data(), kCREDENTIAL_TYPE.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(expiredCredential.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}"]
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH,
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "credentials are expired");
    });
}

TEST_F(RPCDepositAuthorizedTest, DuplicateCredentialsReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30, 34);

    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);
    auto const credential = createCredentialObject(kACCOUNT, kACCOUNT2, kCREDENTIAL_TYPE);
    auto const credentialIndex = ripple::keylet::credential(
                                     getAccountIdWithString(kACCOUNT),
                                     getAccountIdWithString(kACCOUNT2),
                                     ripple::Slice(kCREDENTIAL_TYPE.data(), kCREDENTIAL_TYPE.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}", "{}"]
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH,
        ripple::strHex(credentialIndex),
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "duplicates in credentials.");
    });
}

TEST_F(RPCDepositAuthorizedTest, NoElementsInCredentialsReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30, 34);

    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": []
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "credential array has no elements.");
    });
}

TEST_F(RPCDepositAuthorizedTest, MoreThanMaxNumberOfCredentialsReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30, 34);

    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);
    auto const credential = createCredentialObject(kACCOUNT, kACCOUNT2, kCREDENTIAL_TYPE);
    auto const credentialIndex = ripple::keylet::credential(
                                     getAccountIdWithString(kACCOUNT),
                                     getAccountIdWithString(kACCOUNT2),
                                     ripple::Slice(kCREDENTIAL_TYPE.data(), kCREDENTIAL_TYPE.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<std::string> credentials(9, ripple::strHex(credentialIndex));

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": [{}]
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH,
        fmt::join(
            credentials | std::views::transform([](std::string const& cred) { return fmt::format("\"{}\"", cred); }),
            ", "
        )
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "credential array too long.");
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferenSubjectAccountForCredentialReturnsFalse)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);

    EXPECT_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2);
    auto const account2Root = createAccountRootObject(kACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, kINDEX2, 2);

    // reverse the subject and issuer account. Now subject is Account2
    auto const credential = createCredentialObject(kACCOUNT2, kACCOUNT, kCREDENTIAL_TYPE);
    auto const credentialIndex = ripple::keylet::credential(
                                     getAccountIdWithString(kACCOUNT2),
                                     getAccountIdWithString(kACCOUNT),
                                     ripple::Slice(kCREDENTIAL_TYPE.data(), kCREDENTIAL_TYPE.size())
    )
                                     .key;

    ON_CALL(*backend_, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}"]
        }})",
        kACCOUNT,
        kACCOUNT2,
        kLEDGER_HASH,
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "credentials don't belong to the root account");
    });
}
