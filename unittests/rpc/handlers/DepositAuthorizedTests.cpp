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
#include <rpc/handlers/DepositAuthorized.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto INDEX2 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1";

constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;

using namespace RPC;
namespace json = boost::json;
using namespace testing;

class RPCDepositAuthorizedTest : public HandlerBaseTest
{
};

struct DepositAuthorizedTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct DepositAuthorizedParameterTest : public RPCDepositAuthorizedTest,
                                        public WithParamInterface<DepositAuthorizedTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<DepositAuthorizedTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

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
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCDepositAuthorizedGroup,
    DepositAuthorizedParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    DepositAuthorizedParameterTest::NameGenerator{});

TEST_P(DepositAuthorizedParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});

        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaIntSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_index": {}
            }})",
            ACCOUNT,
            ACCOUNT2,
            RANGEMAX));

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaStringSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_index": "{}"
            }})",
            ACCOUNT,
            ACCOUNT2,
            RANGEMAX));

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, LedgerNotExistViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "source_account": "{}", 
                "destination_account": "{}", 
                "ledger_hash": "{}"
            }})",
            ACCOUNT,
            ACCOUNT2,
            LEDGERHASH));

        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, SourceAccountDoesNotExist)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);

    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        ACCOUNT2,
        LEDGERHASH));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "srcActNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "source_accountNotFound");
    });
}

TEST_F(RPCDepositAuthorizedTest, DestinationAccountDoesNotExist)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);

    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(_, _, _)).WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT2)).key, _, _))
        .WillByDefault(Return(std::optional<Blob>{}));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        ACCOUNT2,
        LEDGERHASH));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
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

    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);

    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(accountRoot.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        ACCOUNT,
        LEDGERHASH));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
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

    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);

    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    auto const account1Root = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    auto const account2Root = CreateAccountRootObject(ACCOUNT2, 0, 2, 200, 2, INDEX2, 2);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        ACCOUNT2,
        LEDGERHASH));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
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

    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);

    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    auto const account1Root = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    auto const account2Root = CreateAccountRootObject(ACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, INDEX2, 2);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::nullopt));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        ACCOUNT2,
        LEDGERHASH));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
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

    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);

    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    auto const account1Root = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    auto const account2Root = CreateAccountRootObject(ACCOUNT2, ripple::lsfDepositAuth, 2, 200, 2, INDEX2, 2);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(_, _, _)).WillByDefault(Return(std::optional<Blob>{{1, 2, 3}}));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key, _, _))
        .WillByDefault(Return(account1Root.getSerializer().peekData()));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT2)).key, _, _))
        .WillByDefault(Return(account2Root.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    auto const input = json::parse(fmt::format(
        R"({{
            "source_account": "{}",
            "destination_account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        ACCOUNT2,
        LEDGERHASH));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{DepositAuthorizedHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}
