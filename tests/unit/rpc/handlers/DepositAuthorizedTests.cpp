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

constexpr static auto Account = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto Account2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto Index1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto Index2 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1";
constexpr static std::string_view CredentialType = "credType";
constexpr static auto CredentialHash = "F245428267E6177AEEFDD4FEA3533285712A4B1091CF82A7EA7BC39A62C3FB1A";

constexpr static auto RangeMin = 10;
constexpr static auto RangeMax = 30;

using namespace rpc;
namespace json = boost::json;
using namespace testing;

struct RPCDepositAuthorizedTest : HandlerBaseTest {
    RPCDepositAuthorizedTest()
    {
        backend->setRange(RangeMin, RangeMax);
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
            "SourceAccountMissing",
            R"({
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            "invalidParams",
            "Required field 'source_account' missing",
        },
        {
            "SourceAccountMalformed",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            "actMalformed",
            "source_accountMalformed",
        },
        {
            "SourceAccountNotString",
            R"({
                "source_account": 1234, 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            "invalidParams",
            "source_accountNotString",
        },
        {
            "DestinationAccountMissing",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            "invalidParams",
            "Required field 'destination_account' missing",
        },
        {
            "DestinationAccountMalformed",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            "actMalformed",
            "destination_accountMalformed",
        },
        {
            "DestinationAccountNotString",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": 1234,
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"
            })",
            "invalidParams",
            "destination_accountNotString",
        },
        {
            "LedgerHashInvalid",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "x"
            })",
            "invalidParams",
            "ledger_hashMalformed",
        },
        {
            "LedgerHashNotString",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": 123
            })",
            "invalidParams",
            "ledger_hashNotString",
        },
        {
            "LedgerIndexNotInt",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_index": "x"
            })",
            "invalidParams",
            "ledgerIndexMalformed",
        },
        {
            "CredentialsNotArray",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "credentials": "x"
            })",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "CredentialsNotStringsInArray",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "credentials": [123]
            })",
            "invalidParams",
            "Item is not a valid uint256 type.",
        },
        {
            "CredentialsNotHexedStringInArray",
            R"({
                "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "credentials": ["234", "432"]
            })",
            "invalidParams",
            "Item is not a valid uint256 type.",
        }
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCDepositAuthorizedGroup,
    DepositAuthorizedParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(DepositAuthorizedParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
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
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence(RangeMax, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_index": {}
            }})",
            Account,
            Account2,
            RangeMax
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
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence(RangeMax, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_index": "{}"
            }})",
            Account,
            Account2,
            RangeMax
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
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_hash": "{}"
            }})",
            Account,
            Account2,
            LedgerHash
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
    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);

    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        Account,
        Account2,
        LedgerHash
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "srcActNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "source_accountNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, DestinationAccountDoesNotExist)
{
    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);

    auto const accountRoot = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(std::optional<Blob>{}));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        Account,
        Account2,
        LedgerHash
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "dstActNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "destination_accountNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, AccountsAreEqual)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
        })";

    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);

    auto const accountRoot = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(accountRoot.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        Account,
        Account,
        LedgerHash
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsNoDepositAuthFlag)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })";

    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, 0, 2, 200, 2, Index2, 2);

    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        Account,
        Account2,
        LedgerHash
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsWithDepositAuthFlagReturnsFalse)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": false,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })";

    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::nullopt));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        Account,
        Account2,
        LedgerHash
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferentAccountsWithDepositAuthFlagReturnsTrue)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        })";

    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        Account,
        Account2,
        LedgerHash
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialAcceptedAndNotExpiredReturnsTrue)
{
    static auto const expectedOut = fmt::format(
        R"({{
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true,
            "deposit_authorized": true,
            "source_account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "destination_account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "credentials": ["{}"]
        }})",
        CredentialHash  // CREDENTIALHASH should match credentialIndex
    );

    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);
    auto const credential = CreateCredentialObject(Account, Account2, CredentialType);
    auto const credentialIndex = ripple::keylet::credential(
                                     GetAccountIDWithString(Account),
                                     GetAccountIDWithString(Account2),
                                     ripple::Slice(CredentialType.data(), CredentialType.size())
    )
                                     .key;

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(4);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}"]
        }})",
        Account,
        Account2,
        LedgerHash,
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOut));
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialNotAuthorizedReturnsFalse)
{
    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);
    auto const credential = CreateCredentialObject(Account, Account2, CredentialType, false);
    auto const credentialIndex = ripple::keylet::credential(
                                     GetAccountIDWithString(Account),
                                     GetAccountIDWithString(Account2),
                                     ripple::Slice(CredentialType.data(), CredentialType.size())
    )
                                     .key;

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}"]
        }})",
        Account,
        Account2,
        LedgerHash,
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "credentials aren't accepted");
    });
}

TEST_F(RPCDepositAuthorizedTest, CredentialExpiredReturnsFalse)
{
    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30, 100);

    // set parent close time to 500 seconds
    ledgerHeader.parentCloseTime = ripple::NetClock::time_point{std::chrono::seconds{500}};

    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);

    // credential expire time is 23 seconds, so credential will fail
    auto const expiredCredential = CreateCredentialObject(Account, Account2, CredentialType, true, 23);

    auto const credentialIndex = ripple::keylet::credential(
                                     GetAccountIDWithString(Account),
                                     GetAccountIDWithString(Account2),
                                     ripple::Slice(CredentialType.data(), CredentialType.size())
    )
                                     .key;

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(expiredCredential.getSerializer().peekData()));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}"]
        }})",
        Account,
        Account2,
        LedgerHash,
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "credentials are expired");
    });
}

TEST_F(RPCDepositAuthorizedTest, DuplicateCredentialsReturnsFalse)
{
    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30, 34);

    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);
    auto const credential = CreateCredentialObject(Account, Account2, CredentialType);
    auto const credentialIndex = ripple::keylet::credential(
                                     GetAccountIDWithString(Account),
                                     GetAccountIDWithString(Account2),
                                     ripple::Slice(CredentialType.data(), CredentialType.size())
    )
                                     .key;

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}", "{}"]
        }})",
        Account,
        Account2,
        LedgerHash,
        ripple::strHex(credentialIndex),
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "duplicates in credentials.");
    });
}

TEST_F(RPCDepositAuthorizedTest, NoElementsInCredentialsReturnsFalse)
{
    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30, 34);

    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": []
        }})",
        Account,
        Account2,
        LedgerHash
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "credential array has no elements.");
    });
}

TEST_F(RPCDepositAuthorizedTest, MoreThanMaxNumberOfCredentialsReturnsFalse)
{
    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30, 34);

    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);
    auto const credential = CreateCredentialObject(Account, Account2, CredentialType);
    auto const credentialIndex = ripple::keylet::credential(
                                     GetAccountIDWithString(Account),
                                     GetAccountIDWithString(Account2),
                                     ripple::Slice(CredentialType.data(), CredentialType.size())
    )
                                     .key;

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);

    std::vector<std::string> credentials(9, ripple::strHex(credentialIndex));

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": [{}]
        }})",
        Account,
        Account2,
        LedgerHash,
        fmt::join(
            credentials | std::views::transform([](std::string const& cred) { return fmt::format("\"{}\"", cred); }),
            ", "
        )
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "credential array too long.");
    });
}

TEST_F(RPCDepositAuthorizedTest, DifferenSubjectAccountForCredentialReturnsFalse)
{
    auto ledgerHeader = CreateLedgerHeader(LedgerHash, 30);

    EXPECT_CALL(*backend, fetchLedgerByHash(ripple::uint256{LedgerHash}, _)).WillOnce(Return(ledgerHeader));

    auto const account1Root = CreateAccountRootObject(Account, 0, 2, 200, 2, Index1, 2);
    auto const account2Root = CreateAccountRootObject(Account2, ripple::lsfDepositAuth, 2, 200, 2, Index2, 2);

    // reverse the subject and issuer account. Now subject is ACCOUNT2
    auto const credential = CreateCredentialObject(Account2, Account, CredentialType);
    auto const credentialIndex = ripple::keylet::credential(
                                     GetAccountIDWithString(Account2),
                                     GetAccountIDWithString(Account),
                                     ripple::Slice(CredentialType.data(), CredentialType.size())
    )
                                     .key;

    ON_CALL(*backend, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(Account2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(credentialIndex, _, _))
        .WillByDefault(Return(credential.getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}",
            "credentials": ["{}"]
        }})",
        Account,
        Account2,
        LedgerHash,
        ripple::strHex(credentialIndex)
    ));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{backend}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "badCredentials");
        EXPECT_EQ(err.at("error_message").as_string(), "credentials don't belong to the root account");
    });
}
