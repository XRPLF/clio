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
#include "rpc/Amendments.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/AccountInfo.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT1 = "rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

class RPCAccountInfoHandlerTest : public HandlerBaseTest {};

struct AccountInfoParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountInfoParameterTest : public RPCAccountInfoHandlerTest,
                                  public WithParamInterface<AccountInfoParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AccountInfoParamTestCaseBundle>{
        AccountInfoParamTestCaseBundle{"MissingAccountAndIdent", R"({})", "invalidParams", "Missing field 'account'."},
        AccountInfoParamTestCaseBundle{"AccountNotString", R"({"account":1})", "invalidParams", "accountNotString"},
        AccountInfoParamTestCaseBundle{"AccountInvalid", R"({"account":"xxx"})", "actMalformed", "accountMalformed"},
        AccountInfoParamTestCaseBundle{"IdentNotString", R"({"ident":1})", "invalidParams", "identNotString"},
        AccountInfoParamTestCaseBundle{"IdentInvalid", R"({"ident":"xxx"})", "actMalformed", "identMalformed"},
        AccountInfoParamTestCaseBundle{
            "SignerListsInvalid",
            R"({"ident":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "signer_lists":1})",
            "invalidParams",
            "Invalid parameters."
        },
        AccountInfoParamTestCaseBundle{
            "LedgerHashInvalid",
            R"({"ident":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":"1"})",
            "invalidParams",
            "ledger_hashMalformed"
        },
        AccountInfoParamTestCaseBundle{
            "LedgerHashNotString",
            R"({"ident":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":1})",
            "invalidParams",
            "ledger_hashNotString"
        },
        AccountInfoParamTestCaseBundle{
            "LedgerIndexInvalid",
            R"({"ident":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_index":"a"})",
            "invalidParams",
            "ledgerIndexMalformed"
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountInfoGroup1,
    AccountInfoParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(AccountInfoParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountInfoHandler{backend}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(AccountInfoParameterTest, ApiV1SignerListIsNotBool)
{
    static constexpr auto reqJson = R"(
        {"ident":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "signer_lists":1}
    )";

    EXPECT_CALL(*backend, fetchLedgerBySequence);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountInfoHandler{backend}};
        auto const req = json::parse(reqJson);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 1});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountInfoHandlerTest, LedgerNonExistViaIntSequence)
{
    backend->setRange(10, 30);

    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend, fetchLedgerBySequence(30, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_index": 30
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountInfoHandlerTest, LedgerNonExistViaStringSequence)
{
    backend->setRange(10, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend, fetchLedgerBySequence(30, _)).WillByDefault(Return(std::nullopt));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_index": "30"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountInfoHandlerTest, LedgerNonExistViaHash)
{
    backend->setRange(10, 30);
    EXPECT_CALL(*backend, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountInfoHandlerTest, AccountNotExist)
{
    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAccountInfoHandlerTest, AccountInvalid)
{
    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    // return a valid ledger object but not account root
    ON_CALL(*backend, doFetchLedgerObject).WillByDefault(Return(CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(1);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "dbDeserialization");
        EXPECT_EQ(err.at("error_message").as_string(), "Database deserialization error.");
    });
}

TEST_F(RPCAccountInfoHandlerTest, SignerListsInvalid)
{
    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);

    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    auto signersKey = ripple::keylet::signers(account).key;
    ON_CALL(*backend, doFetchLedgerObject(signersKey, 30, _))
        .WillByDefault(Return(CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, 30, _))
        .WillByDefault(Return(CreateAmendmentsObject({}).getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(4);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "signer_lists": true
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "dbDeserialization");
        EXPECT_EQ(err.at("error_message").as_string(), "Database deserialization error.");
    });
}

TEST_F(RPCAccountInfoHandlerTest, SignerListsTrueV2)
{
    auto const expectedOutput = fmt::format(
        R"({{
            "account_data": 
            {{
                "Account": "{}",
                "Balance": "200",
                "Flags": 0,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 2,
                "PreviousTxnID": "{}",
                "PreviousTxnLgrSeq": 2,
                "Sequence": 2,
                "TransferRate": 0,
                "index": "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8"
            }},
            "signer_lists":
            [
                {{
                    "Flags": 0,
                    "LedgerEntryType": "SignerList",
                    "OwnerNode": "0",
                    "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                    "PreviousTxnLgrSeq": 0,
                    "SignerEntries":
                    [
                        {{
                            "SignerEntry":
                            {{
                                "Account": "{}",
                                "SignerWeight": 1
                            }}
                        }},
                        {{
                            "SignerEntry":
                            {{
                                "Account": "{}",
                                "SignerWeight": 1
                            }}
                        }}
                    ],
                    "SignerListID": 0,
                    "SignerQuorum": 2,
                    "index": "A9C28A28B85CD533217F5C0A0C7767666B093FA58A0F2D80026FCC4CD932DDC7"
                }}
            ],
            "account_flags": 
            {{
                "defaultRipple": false,
                "depositAuth": false,
                "disableMasterKey": false,
                "disallowIncomingXRP": false,
                "globalFreeze": false,
                "noFreeze": false,
                "passwordSpent": false,
                "requireAuthorization": false,
                "requireDestinationTag": false
            }},
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true
        }})",
        ACCOUNT,
        INDEX1,
        ACCOUNT1,
        ACCOUNT2,
        LEDGERHASH
    );

    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    auto signersKey = ripple::keylet::signers(account).key;
    ON_CALL(*backend, doFetchLedgerObject(signersKey, 30, _))
        .WillByDefault(Return(CreateSignerLists({{ACCOUNT1, 1}, {ACCOUNT2, 1}}).getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, 30, _))
        .WillByDefault(Return(CreateAmendmentsObject({}).getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(4);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "signer_lists": true
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{.yield = yield, .apiVersion = 2});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountInfoHandlerTest, SignerListsTrueV1)
{
    auto const expectedOutput = fmt::format(
        R"({{
            "account_data": 
            {{
                "Account": "{}",
                "Balance": "200",
                "Flags": 0,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 2,
                "PreviousTxnID": "{}",
                "PreviousTxnLgrSeq": 2,
                "Sequence": 2,
                "TransferRate": 0,
                "index": "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8",
                "signer_lists":
                [
                    {{
                        "Flags": 0,
                        "LedgerEntryType": "SignerList",
                        "OwnerNode": "0",
                        "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                        "PreviousTxnLgrSeq": 0,
                        "SignerEntries":
                        [
                            {{
                                "SignerEntry":
                                {{
                                    "Account": "{}",
                                    "SignerWeight": 1
                                }}
                            }},
                            {{
                                "SignerEntry":
                                {{
                                    "Account": "{}",
                                    "SignerWeight": 1
                                }}
                            }}
                        ],
                        "SignerListID": 0,
                        "SignerQuorum": 2,
                        "index": "A9C28A28B85CD533217F5C0A0C7767666B093FA58A0F2D80026FCC4CD932DDC7"
                    }}
                ]
            }},
            "account_flags": 
            {{
                "defaultRipple": false,
                "depositAuth": false,
                "disableMasterKey": false,
                "disallowIncomingXRP": false,
                "globalFreeze": false,
                "noFreeze": false,
                "passwordSpent": false,
                "requireAuthorization": false,
                "requireDestinationTag": false
            }},
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true
        }})",
        ACCOUNT,
        INDEX1,
        ACCOUNT1,
        ACCOUNT2,
        LEDGERHASH
    );

    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    auto signersKey = ripple::keylet::signers(account).key;
    ON_CALL(*backend, doFetchLedgerObject(signersKey, 30, _))
        .WillByDefault(Return(CreateSignerLists({{ACCOUNT1, 1}, {ACCOUNT2, 1}}).getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, 30, _))
        .WillByDefault(Return(CreateAmendmentsObject({}).getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(4);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "signer_lists": true
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{.yield = yield, .apiVersion = 1});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountInfoHandlerTest, Flags)
{
    auto const expectedOutput = fmt::format(
        R"({{
            "account_data": {{
                "Account": "{}",
                "Balance": "200",
                "Flags": 33488896,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 2,
                "PreviousTxnID": "{}",
                "PreviousTxnLgrSeq": 2,
                "Sequence": 2,
                "TransferRate": 0,
                "index": "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8"
            }},
            "account_flags": {{
                "defaultRipple": true,
                "depositAuth": true,
                "disableMasterKey": true,
                "disallowIncomingXRP": true,
                "globalFreeze": true,
                "noFreeze": true,
                "passwordSpent": true,
                "requireAuthorization": true,
                "requireDestinationTag": true
            }},
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true
        }})",
        ACCOUNT,
        INDEX1,
        LEDGERHASH
    );

    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(
        ACCOUNT,
        ripple::lsfDefaultRipple | ripple::lsfGlobalFreeze | ripple::lsfRequireDestTag | ripple::lsfRequireAuth |
            ripple::lsfDepositAuth | ripple::lsfDisableMaster | ripple::lsfDisallowXRP | ripple::lsfNoFreeze |
            ripple::lsfPasswordSpent,
        2,
        200,
        2,
        INDEX1,
        2
    );
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, 30, _))
        .WillByDefault(Return(CreateAmendmentsObject({}).getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountInfoHandlerTest, IdentAndSignerListsFalse)
{
    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, 30, _))
        .WillByDefault(Return(CreateAmendmentsObject({}).getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto static const input = json::parse(fmt::format(
        R"({{
            "ident": "{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output.result->as_object().contains("signer_lists"));
    });
}

TEST_F(RPCAccountInfoHandlerTest, DisallowIncoming)
{
    auto const expectedOutput = fmt::format(
        R"({{
            "account_data": {{
                "Account": "{}",
                "Balance": "200",
                "Flags": 1040121856,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 2,
                "PreviousTxnID": "{}",
                "PreviousTxnLgrSeq": 2,
                "Sequence": 2,
                "TransferRate": 0,
                "index": "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8"
            }},
            "account_flags": {{
                "defaultRipple": true,
                "depositAuth": true,
                "disableMasterKey": true,
                "disallowIncomingXRP": true,
                "globalFreeze": true,
                "noFreeze": true,
                "passwordSpent": true,
                "requireAuthorization": true,
                "requireDestinationTag": true,
                "disallowIncomingCheck": true,
                "disallowIncomingNFTokenOffer": true,
                "disallowIncomingPayChan": true,
                "disallowIncomingTrustline": true
            }},
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true
        }})",
        ACCOUNT,
        INDEX1,
        LEDGERHASH
    );

    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(
        ACCOUNT,
        ripple::lsfDefaultRipple | ripple::lsfGlobalFreeze | ripple::lsfRequireDestTag | ripple::lsfRequireAuth |
            ripple::lsfDepositAuth | ripple::lsfDisableMaster | ripple::lsfDisallowXRP | ripple::lsfNoFreeze |
            ripple::lsfPasswordSpent | ripple::lsfDisallowIncomingNFTokenOffer | ripple::lsfDisallowIncomingCheck |
            ripple::lsfDisallowIncomingPayChan | ripple::lsfDisallowIncomingTrustline,
        2,
        200,
        2,
        INDEX1,
        2
    );
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, 30, _))
        .WillByDefault(Return(CreateAmendmentsObject({rpc::Amendments::DisallowIncoming}).getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountInfoHandlerTest, Clawback)
{
    auto const expectedOutput = fmt::format(
        R"({{
            "account_data": {{
                "Account": "{}",
                "Balance": "200",
                "Flags": 2180972544,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 2,
                "PreviousTxnID": "{}",
                "PreviousTxnLgrSeq": 2,
                "Sequence": 2,
                "TransferRate": 0,
                "index": "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8"
            }},
            "account_flags": {{
                "defaultRipple": true,
                "depositAuth": true,
                "disableMasterKey": true,
                "disallowIncomingXRP": true,
                "globalFreeze": true,
                "noFreeze": true,
                "passwordSpent": true,
                "requireAuthorization": true,
                "requireDestinationTag": true,
                "allowTrustLineClawback": true
            }},
            "ledger_hash": "{}",
            "ledger_index": 30,
            "validated": true
        }})",
        ACCOUNT,
        INDEX1,
        LEDGERHASH
    );

    backend->setRange(10, 30);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(
        ACCOUNT,
        ripple::lsfDefaultRipple | ripple::lsfGlobalFreeze | ripple::lsfRequireDestTag | ripple::lsfRequireAuth |
            ripple::lsfDepositAuth | ripple::lsfDisableMaster | ripple::lsfDisallowXRP | ripple::lsfNoFreeze |
            ripple::lsfPasswordSpent | ripple::lsfAllowTrustLineClawback,
        2,
        200,
        2,
        INDEX1,
        2
    );
    ON_CALL(*backend, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, 30, _))
        .WillByDefault(Return(CreateAmendmentsObject({rpc::Amendments::Clawback}).getSerializer().peekData()));
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);

    auto static const input = json::parse(fmt::format(
        R"({{
            "account": "{}"
        }})",
        ACCOUNT
    ));
    auto const handler = AnyHandler{AccountInfoHandler{backend}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(expectedOutput));
    });
}

TEST(RPCAccountInfoHandlerSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"account", ACCOUNT},
        {"ident", ACCOUNT},
        {"ledger_index", 30},
        {"ledger_hash", LEDGERHASH},
        {"ledger", "some"},
        {"strict", true}
    };
    auto const spec = AccountInfoHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    auto const& warning = warnings[0];
    ASSERT_TRUE(warning.is_object());
    auto const obj = warning.as_object();
    ASSERT_TRUE(obj.contains("id"));
    ASSERT_TRUE(obj.contains("message"));
    EXPECT_EQ(obj.at("id").as_int64(), static_cast<int64_t>(WarningCode::warnRPC_DEPRECATED));
    auto const& message = obj.at("message").as_string();
    for (auto const& field : {"ident", "ledger", "strict"}) {
        EXPECT_NE(message.find(fmt::format("Field '{}' is deprecated", field)), std::string::npos) << message;
    }
}
