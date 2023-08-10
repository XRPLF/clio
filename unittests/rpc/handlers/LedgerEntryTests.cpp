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
#include <rpc/handlers/LedgerEntry.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto INDEX1 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto TOKENID = "000827103B94ECBB7BF0A0A6ED62B3607801A27B65F4679F4AD1D4850000C0EA";

class RPCLedgerEntryTest : public HandlerBaseTest
{
};

struct ParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct LedgerEntryParameterTest : public RPCLedgerEntryTest, public WithParamInterface<ParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<ParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

// TODO: because we extract the error generation from the handler to framework
// the error messages need one round fine tuning
static auto
generateTestValuesForParametersTest()
{
    return std::vector<ParamTestCaseBundle>{
        ParamTestCaseBundle{
            "InvalidBinaryType",
            R"({
                "index":
                "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
                "binary": "invalid"
            })",
            "invalidParams",
            "Invalid parameters."},

        ParamTestCaseBundle{
            "InvalidAccountRootFormat",
            R"({
                "account_root": "invalid"
            })",
            "malformedAddress",
            "Malformed address."},

        ParamTestCaseBundle{
            "InvalidAccountRootNotString",
            R"({
                "account_root": 123
            })",
            "invalidParams",
            "account_rootNotString"},

        ParamTestCaseBundle{
            "InvalidLedgerIndex",
            R"({
                "ledger_index": "wrong"
            })",
            "invalidParams",
            "ledgerIndexMalformed"},

        ParamTestCaseBundle{"UnknownOption", R"({})", "unknownOption", "Unknown option."},

        ParamTestCaseBundle{
            "InvalidDepositPreauthType",
            R"({
                "deposit_preauth": 123
            })",
            "invalidParams",
            "Invalid parameters."},

        ParamTestCaseBundle{
            "InvalidDepositPreauthString",
            R"({
                "deposit_preauth": "invalid"
            })",
            "malformedRequest",
            "Malformed request."},

        ParamTestCaseBundle{
            "InvalidDepositPreauthEmtpyJson",
            R"({
                "deposit_preauth": {}
            })",
            "invalidParams",
            "Required field 'owner' missing"},

        ParamTestCaseBundle{
            "InvalidDepositPreauthJsonWrongAccount",
            R"({
                "deposit_preauth": {
                    "owner": "invalid",
                    "authorized": "invalid"
                }
            })",
            "malformedOwner",
            "Malformed owner."},

        ParamTestCaseBundle{
            "InvalidDepositPreauthJsonOwnerNotString",
            R"({
                "deposit_preauth": {
                    "owner": 123,
                    "authorized": 123
                }
            })",
            "malformedOwner",
            "Malformed owner."},

        ParamTestCaseBundle{
            "InvalidDepositPreauthJsonAuthorizedNotString",
            fmt::format(
                R"({{
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized": 123
                    }}
                }})",
                ACCOUNT),
            "invalidParams",
            "authorizedNotString"},

        ParamTestCaseBundle{
            "InvalidTicketType",
            R"({
                "ticket": 123
            })",
            "invalidParams",
            "Invalid parameters."},

        ParamTestCaseBundle{
            "InvalidTicketIndex",
            R"({
                "ticket": "invalid"
            })",
            "malformedRequest",
            "Malformed request."},

        ParamTestCaseBundle{
            "InvalidTicketEmptyJson",
            R"({
                "ticket": {}
            })",
            "invalidParams",
            "Required field 'account' missing"},

        ParamTestCaseBundle{
            "InvalidTicketJsonAccountNotString",
            R"({
                "ticket": {
                    "account": 123,
                    "ticket_seq": 123
                }
            })",
            "invalidParams",
            "accountNotString"},

        ParamTestCaseBundle{
            "InvalidTicketJsonAccountInvalid",
            R"({
                "ticket": {
                    "account": "123",
                    "ticket_seq": 123
                }
            })",
            "malformedAddress",
            "Malformed address."},

        ParamTestCaseBundle{
            "InvalidTicketJsonSeqNotInt",
            fmt::format(
                R"({{
                    "ticket": {{
                        "account": "{}",
                        "ticket_seq": "123"
                    }}
                }})",
                ACCOUNT),
            "malformedRequest",
            "Malformed request."},

        ParamTestCaseBundle{
            "InvalidOfferType",
            R"({
                "offer": 123
            })",
            "invalidParams",
            "Invalid parameters."},

        ParamTestCaseBundle{
            "InvalidOfferIndex",
            R"({
                "offer": "invalid"
            })",
            "malformedRequest",
            "Malformed request."},

        ParamTestCaseBundle{
            "InvalidOfferEmptyJson",
            R"({
                "offer": {}
            })",
            "invalidParams",
            "Required field 'account' missing"},

        ParamTestCaseBundle{
            "InvalidOfferJsonAccountNotString",
            R"({
                "ticket": {
                    "account": 123,
                    "seq": 123
                }
            })",
            "invalidParams",
            "accountNotString"},

        ParamTestCaseBundle{
            "InvalidOfferJsonAccountInvalid",
            R"({
                "ticket": {
                    "account": "123",
                    "seq": 123
                }
            })",
            "malformedAddress",
            "Malformed address."},

        ParamTestCaseBundle{
            "InvalidOfferJsonSeqNotInt",
            fmt::format(
                R"({{
                    "offer": {{
                        "account": "{}",
                        "seq": "123"
                    }}
                }})",
                ACCOUNT),
            "malformedRequest",
            "Malformed request."},

        ParamTestCaseBundle{
            "InvalidEscrowType",
            R"({
                "escrow": 123
            })",
            "invalidParams",
            "Invalid parameters."},

        ParamTestCaseBundle{
            "InvalidEscrowIndex",
            R"({
                "escrow": "invalid"
            })",
            "malformedRequest",
            "Malformed request."},

        ParamTestCaseBundle{
            "InvalidEscrowEmptyJson",
            R"({
                "escrow": {}
            })",
            "invalidParams",
            "Required field 'owner' missing"},

        ParamTestCaseBundle{
            "InvalidEscrowJsonAccountNotString",
            R"({
                "escrow": {
                    "owner": 123,
                    "seq": 123
                }
            })",
            "malformedOwner",
            "Malformed owner."},

        ParamTestCaseBundle{
            "InvalidEscrowJsonAccountInvalid",
            R"({
                "escrow": {
                    "owner": "123",
                    "seq": 123
                }
            })",
            "malformedOwner",
            "Malformed owner."},

        ParamTestCaseBundle{
            "InvalidEscrowJsonSeqNotInt",
            fmt::format(
                R"({{
                    "escrow": {{
                        "owner": "{}",
                        "seq": "123"
                    }}
                }})",
                ACCOUNT),
            "malformedRequest",
            "Malformed request."},

        ParamTestCaseBundle{
            "InvalidRippleStateType",
            R"({
                "ripple_state": "123"
            })",
            "invalidParams",
            "Invalid parameters."},

        ParamTestCaseBundle{
            "InvalidRippleStateMissField",
            R"({
                "ripple_state": {
                    "currency": "USD"
                }
            })",
            "invalidParams",
            "Required field 'accounts' missing"},

        ParamTestCaseBundle{
            "InvalidRippleStateEmtpyJson",
            R"({
                "ripple_state": {}
            })",
            "invalidParams",
            "Required field 'accounts' missing"},

        ParamTestCaseBundle{
            "InvalidRippleStateOneAccount",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}"]
                    }}
                }})",
                ACCOUNT),
            "invalidParams",
            "malformedAccounts"},

        ParamTestCaseBundle{
            "InvalidRippleStateSameAccounts",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT),
            "invalidParams",
            "malformedAccounts"},

        ParamTestCaseBundle{
            "InvalidRippleStateWrongAccountsNotString",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}",123],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT),
            "invalidParams",
            "malformedAccounts"},

        ParamTestCaseBundle{
            "InvalidRippleStateWrongAccountsFormat",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","123"],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT),
            "malformedAddress",
            "malformedAddresses"},

        ParamTestCaseBundle{
            "InvalidRippleStateWrongCurrency",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": "XXXX"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2),
            "malformedCurrency",
            "malformedCurrency"},

        ParamTestCaseBundle{
            "InvalidRippleStateWrongCurrencyNotString",
            fmt::format(
                R"({{
                    "ripple_state": {{
                        "accounts" : ["{}","{}"],
                        "currency": 123
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2),
            "invalidParams",
            "currencyNotString"},

        ParamTestCaseBundle{
            "InvalidDirectoryType",
            R"({
                "directory": 123
            })",
            "invalidParams",
            "Invalid parameters."},

        ParamTestCaseBundle{
            "InvalidDirectoryIndex",
            R"({
                "directory": "123"
            })",
            "malformedRequest",
            "Malformed request."},

        ParamTestCaseBundle{
            "InvalidDirectoryEmtpyJson",
            R"({
                "directory": {}
            })",
            "invalidParams",
            "missingOwnerOrDirRoot"},

        ParamTestCaseBundle{
            "InvalidDirectoryWrongOwnerNotString",
            R"({
                "directory": {
                    "owner": 123
                }
            })",
            "invalidParams",
            "ownerNotString"},

        ParamTestCaseBundle{
            "InvalidDirectoryWrongOwnerFormat",
            R"({
                "directory": {
                    "owner": "123"
                }
            })",
            "malformedAddress",
            "Malformed address."},

        ParamTestCaseBundle{
            "InvalidDirectoryWrongDirFormat",
            R"({
                "directory": {
                    "dir_root": "123"
                }
            })",
            "invalidParams",
            "dir_rootMalformed"},

        ParamTestCaseBundle{
            "InvalidDirectoryWrongDirNotString",
            R"({
                "directory": {
                    "dir_root": 123
                }
            })",
            "invalidParams",
            "dir_rootNotString"},

        ParamTestCaseBundle{
            "InvalidDirectoryDirOwnerConflict",
            fmt::format(
                R"({{
                    "directory": {{
                        "dir_root": "{}",
                        "owner": "{}"
                    }}
                }})",
                INDEX1,
                ACCOUNT),
            "invalidParams",
            "mayNotSpecifyBothDirRootAndOwner"},

        ParamTestCaseBundle{
            "InvalidDirectoryDirSubIndexNotInt",
            fmt::format(
                R"({{
                    "directory": {{
                        "dir_root": "{}",
                        "sub_index": "not int"
                    }}
                }})",
                INDEX1),
            "malformedRequest",
            "Malformed request."},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup1,
    LedgerEntryParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    LedgerEntryParameterTest::NameGenerator{});

TEST_P(LedgerEntryParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

// parameterized test cases for index
struct IndexTest : public HandlerBaseTest, public WithParamInterface<std::string>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            return static_cast<std::string>(info.param);
        }
    };
};

// content of index, payment_channel, nft_page and check fields is ledger index.
INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup3,
    IndexTest,
    Values("index", "nft_page", "payment_channel", "check"),
    IndexTest::NameGenerator{});

TEST_P(IndexTest, InvalidIndexUint256)
{
    auto const index = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "{}": "invalid"
            }})",
            index));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "malformedRequest");
        EXPECT_EQ(err.at("error_message").as_string(), "Malformed request.");
    });
}

TEST_P(IndexTest, InvalidIndexNotString)
{
    auto const index = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "{}": 123
            }})",
            index));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "malformedRequest");
        EXPECT_EQ(err.at("error_message").as_string(), "Malformed request.");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerEntryNotFound)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // return null for ledger entry
    auto const key = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(key, RANGEMAX, _)).WillByDefault(Return(std::optional<Blob>{}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "account_root": "{}"
            }})",
            ACCOUNT));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "entryNotFound");
    });
}

struct NormalPathTestBundle
{
    std::string testName;
    std::string testJson;
    ripple::uint256 expectedIndex;
    ripple::STObject mockedEntity;
};

struct RPCLedgerEntryNormalPathTest : public RPCLedgerEntryTest, public WithParamInterface<NormalPathTestBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<NormalPathTestBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForNormalPathTest()
{
    auto account1 = GetAccountIDWithString(ACCOUNT);
    auto account2 = GetAccountIDWithString(ACCOUNT2);
    ripple::Currency currency;
    ripple::to_currency(currency, "USD");

    return std::vector<NormalPathTestBundle>{
        NormalPathTestBundle{
            "Index",
            fmt::format(
                R"({{
                    "binary": true,
                    "index": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreateAccountRootObject(ACCOUNT2, ripple::lsfGlobalFreeze, 1, 10, 2, INDEX1, 3)},
        NormalPathTestBundle{
            "Payment_channel",
            fmt::format(
                R"({{
                    "binary": true,
                    "payment_channel": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400)},
        NormalPathTestBundle{
            "Nft_page",
            fmt::format(
                R"({{
                    "binary": true,
                    "nft_page": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreateNFTTokenPage(
                std::vector{std::make_pair<std::string, std::string>(TOKENID, "www.ok.com")}, std::nullopt)},
        NormalPathTestBundle{
            "Check",
            fmt::format(
                R"({{
                    "binary": true,
                    "check": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreateCheckLedgerObject(ACCOUNT, ACCOUNT2)},
        NormalPathTestBundle{
            "DirectoryIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "directory": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)},
        NormalPathTestBundle{
            "OfferIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "offer": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreateOfferLedgerObject(
                ACCOUNT, 100, 200, "USD", "XRP", ACCOUNT2, ripple::toBase58(ripple::xrpAccount()), INDEX1)},
        NormalPathTestBundle{
            "EscrowIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "escrow": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreateEscrowLedgerObject(ACCOUNT, ACCOUNT2)},
        NormalPathTestBundle{
            "TicketIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "ticket": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreateTicketLedgerObject(ACCOUNT, 0)},
        NormalPathTestBundle{
            "DepositPreauthIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "deposit_preauth": "{}"
                }})",
                INDEX1),
            ripple::uint256{INDEX1},
            CreateDepositPreauthLedgerObject(ACCOUNT, ACCOUNT2)},
        NormalPathTestBundle{
            "AccountRoot",
            fmt::format(
                R"({{
                    "binary": true,
                    "account_root": "{}"
                }})",
                ACCOUNT),
            ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key,
            CreateAccountRootObject(ACCOUNT, 0, 1, 1, 1, INDEX1, 1)},
        NormalPathTestBundle{
            "DirectoryViaDirRoot",
            fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "dir_root": "{}",
                        "sub_index": 2
                    }}
                }})",
                INDEX1),
            ripple::keylet::page(ripple::uint256{INDEX1}, 2).key,
            CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)},
        NormalPathTestBundle{
            "DirectoryViaOwner",
            fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "owner": "{}",
                        "sub_index": 2
                    }}
                }})",
                ACCOUNT),
            ripple::keylet::page(ripple::keylet::ownerDir(account1), 2).key,
            CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)},
        NormalPathTestBundle{
            "DirectoryViaDefaultSubIndex",
            fmt::format(
                R"({{
                    "binary": true,
                    "directory": {{
                        "owner": "{}"
                    }}
                }})",
                ACCOUNT),
            // default sub_index is 0
            ripple::keylet::page(ripple::keylet::ownerDir(account1), 0).key,
            CreateOwnerDirLedgerObject(std::vector<ripple::uint256>{ripple::uint256{INDEX1}}, INDEX1)},
        NormalPathTestBundle{
            "Escrow",
            fmt::format(
                R"({{
                    "binary": true,
                    "escrow": {{
                        "owner": "{}",
                        "seq": 1
                    }}
                }})",
                ACCOUNT),
            ripple::keylet::escrow(account1, 1).key,
            CreateEscrowLedgerObject(ACCOUNT, ACCOUNT2)},
        NormalPathTestBundle{
            "DepositPreauth",
            fmt::format(
                R"({{
                    "binary": true,
                    "deposit_preauth": {{
                        "owner": "{}",
                        "authorized": "{}"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2),
            ripple::keylet::depositPreauth(account1, account2).key,
            CreateDepositPreauthLedgerObject(ACCOUNT, ACCOUNT2)},
        NormalPathTestBundle{
            "RippleState",
            fmt::format(
                R"({{
                    "binary": true,
                    "ripple_state": {{
                        "accounts": ["{}","{}"],
                        "currency": "USD"
                    }}
                }})",
                ACCOUNT,
                ACCOUNT2),
            ripple::keylet::line(account1, account2, currency).key,
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 100, ACCOUNT, 10, ACCOUNT2, 20, INDEX1, 123, 0)},
        NormalPathTestBundle{
            "Ticket",
            fmt::format(
                R"({{
                    "binary": true,
                    "ticket": {{
                        "account": "{}",
                        "ticket_seq": 2
                    }}
                }})",
                ACCOUNT),
            ripple::getTicketIndex(account1, 2),
            CreateTicketLedgerObject(ACCOUNT, 0)},
        NormalPathTestBundle{
            "Offer",
            fmt::format(
                R"({{
                    "binary": true,
                    "offer": {{
                        "account": "{}",
                        "seq": 2
                    }}
                }})",
                ACCOUNT),
            ripple::keylet::offer(account1, 2).key,
            CreateOfferLedgerObject(
                ACCOUNT, 100, 200, "USD", "XRP", ACCOUNT2, ripple::toBase58(ripple::xrpAccount()), INDEX1)}};
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerEntryGroup2,
    RPCLedgerEntryNormalPathTest,
    ValuesIn(generateTestValuesForNormalPathTest()),
    RPCLedgerEntryNormalPathTest::NameGenerator{});

// Test for normal path
// Check the index in response matches the computed index accordingly
TEST_P(RPCLedgerEntryNormalPathTest, NormalPath)
{
    auto const testBundle = GetParam();
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(testBundle.expectedIndex, RANGEMAX, _))
        .WillByDefault(Return(testBundle.mockedEntity.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.value().at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output.value().at("ledger_index").as_uint64(), RANGEMAX);
        EXPECT_EQ(
            output.value().at("node_binary").as_string(),
            ripple::strHex(testBundle.mockedEntity.getSerializer().peekData()));
        EXPECT_EQ(ripple::uint256(output.value().at("index").as_string().c_str()), testBundle.expectedIndex);
    });
}

// this testcase will test the deserialization of ledger entry
TEST_F(RPCLedgerEntryTest, BinaryFalse)
{
    static auto constexpr OUT = R"({
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index":30,
        "validated":true,
        "index":"05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "node":{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount":"100",
            "Balance":"200",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Flags":0,
            "LedgerEntryType":"PayChannel",
            "OwnerNode":"0",
            "PreviousTxnID":"05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
            "PreviousTxnLgrSeq":400,
            "PublicKey":"020000000000000000000000000000000000000000000000000000000000000000",
            "SettleDelay":300,
            "index":"05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD"
        }
    })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400);
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillByDefault(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "payment_channel": "{}"
            }})",
            INDEX1));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(OUT));
    });
}

TEST_F(RPCLedgerEntryTest, UnexpectedLedgerType)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // return valid ledger entry which can be deserialized
    auto const ledgerEntry = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 200, 300, INDEX1, 400);
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::uint256{INDEX1}, RANGEMAX, _))
        .WillByDefault(Return(ledgerEntry.getSerializer().peekData()));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "check": "{}"
            }})",
            INDEX1));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "unexpectedLedgerType");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaIntSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "check": "{}",
                "ledger_index": {}
            }})",
            INDEX1,
            RANGEMAX));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaStringSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "check": "{}",
                "ledger_index": "{}"
            }})",
            INDEX1,
            RANGEMAX));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerEntryTest, LedgerNotExistViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "check": "{}",
                "ledger_hash": "{}"
            }})",
            INDEX1,
            LEDGERHASH));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}
