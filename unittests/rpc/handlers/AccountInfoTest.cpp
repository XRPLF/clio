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
#include <rpc/handlers/AccountInfo.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT1 = "rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

class RPCAccountInfoHandlerTest : public HandlerBaseTest
{
};

struct AccountInfoParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountInfoParameterTest : public RPCAccountInfoHandlerTest,
                                  public WithParamInterface<AccountInfoParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<AccountInfoParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

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
            "Invalid parameters."},
        AccountInfoParamTestCaseBundle{
            "LedgerHashInvalid",
            R"({"ident":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":"1"})",
            "invalidParams",
            "ledger_hashMalformed"},
        AccountInfoParamTestCaseBundle{
            "LedgerHashNotString",
            R"({"ident":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":1})",
            "invalidParams",
            "ledger_hashNotString"},
        AccountInfoParamTestCaseBundle{
            "LedgerIndexInvalid",
            R"({"ident":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_index":"a"})",
            "invalidParams",
            "ledgerIndexMalformed"},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountInfoGroup1,
    AccountInfoParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    AccountInfoParameterTest::NameGenerator{});

TEST_P(AccountInfoParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAccountInfoHandlerTest, LedgerNonExistViaIntSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(30, _)).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":30
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountInfoHandlerTest, LedgerNonExistViaStringSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(30, _)).WillByDefault(Return(std::nullopt));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"30"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountInfoHandlerTest, LedgerNonExistViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountInfoHandlerTest, AccountNotExist)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Account not found.");
    });
}

TEST_F(RPCAccountInfoHandlerTest, AccountInvalid)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    // return a valid ledger object but not account root
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(CreateFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "dbDeserialization");
        EXPECT_EQ(err.at("error_message").as_string(), "Database deserialization error.");
    });
}

TEST_F(RPCAccountInfoHandlerTest, SignerListsInvalid)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    auto signersKey = ripple::keylet::signers(account).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(signersKey, 30, _))
        .WillByDefault(Return(CreateFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "signer_lists":true
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "dbDeserialization");
        EXPECT_EQ(err.at("error_message").as_string(), "Database deserialization error.");
    });
}

TEST_F(RPCAccountInfoHandlerTest, SignerListsTrue)
{
    auto const expectedOutput = fmt::format(
        R"({{
            "account_data":{{
                "Account":"{}",
                "Balance":"200",
                "Flags":0,
                "LedgerEntryType":"AccountRoot",
                "OwnerCount":2,
                "PreviousTxnID":"{}",
                "PreviousTxnLgrSeq":2,
                "Sequence":2,
                "TransferRate":0,
                "index":"13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8"
            }},
            "signer_lists":
                [
                    {{
                        "Flags":0,
                        "LedgerEntryType":"SignerList",
                        "OwnerNode":"0",
                        "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                        "PreviousTxnLgrSeq":0,
                        "SignerEntries":
                        [
                            {{
                                "SignerEntry":
                                {{
                                    "Account":"{}",
                                    "SignerWeight":1
                                }}
                            }},
                            {{
                                "SignerEntry":
                                {{
                                    "Account":"{}",
                                    "SignerWeight":1
                                }}
                            }}
                        ],
                        "SignerListID":0,
                        "SignerQuorum":2,
                        "index":"A9C28A28B85CD533217F5C0A0C7767666B093FA58A0F2D80026FCC4CD932DDC7"
                    }}
                ],
            "account_flags": {{
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
            "ledger_hash":"{}",
            "ledger_index":30,
            "validated":true
        }})",
        ACCOUNT,
        INDEX1,
        ACCOUNT1,
        ACCOUNT2,
        LEDGERHASH);
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    auto signersKey = ripple::keylet::signers(account).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(signersKey, 30, _))
        .WillByDefault(Return(CreateSignerLists({{ACCOUNT1, 1}, {ACCOUNT2, 1}}).getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "signer_lists":true
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountInfoHandlerTest, Flags)
{
    auto const expectedOutput = fmt::format(
        R"({{
            "account_data":{{
                "Account":"{}",
                "Balance":"200",
                "Flags":33488896,
                "LedgerEntryType":"AccountRoot",
                "OwnerCount":2,
                "PreviousTxnID":"{}",
                "PreviousTxnLgrSeq":2,
                "Sequence":2,
                "TransferRate":0,
                "index":"13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8"
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
            "ledger_hash":"{}",
            "ledger_index":30,
            "validated":true
        }})",
        ACCOUNT,
        INDEX1,
        LEDGERHASH);
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

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
        2);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOutput));
    });
}

TEST_F(RPCAccountInfoHandlerTest, IdentAndSignerListsFalse)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

    auto const account = GetAccountIDWithString(ACCOUNT);
    auto const accountKk = ripple::keylet::account(account).key;
    auto const accountRoot = CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, 30, _))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "ident":"{}"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountInfoHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output->as_object().contains("signer_lists"));
    });
}
