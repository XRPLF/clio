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
#include <ripple/protocol/TxFlags.h>
#include <rpc/common/AnyHandler.h>
#include <rpc/handlers/NoRippleCheck.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto ISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr static auto TXNID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";

class RPCNoRippleCheckTest : public HandlerBaseTest
{
};

struct NoRippleParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct NoRippleCheckParameterTest : public RPCNoRippleCheckTest, public WithParamInterface<NoRippleParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<NoRippleParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<NoRippleParamTestCaseBundle>{
        NoRippleParamTestCaseBundle{
            "AccountNotExists",
            R"({
                "role": "gateway"
             })",
            "invalidParams",
            "Required field 'account' missing"},
        NoRippleParamTestCaseBundle{
            "AccountNotString",
            R"({
                "account": 123,
                "role": "gateway"
             })",
            "invalidParams",
            "accountNotString"},
        NoRippleParamTestCaseBundle{
            "InvalidAccount",
            R"({
                "account": "123",
                "role": "gateway"
             })",
            "actMalformed",
            "accountMalformed"},
        NoRippleParamTestCaseBundle{
            "InvalidRole",
            R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "notrole"
             })",
            "invalidParams",
            "role field is invalid"},
        NoRippleParamTestCaseBundle{
            "RoleNotExists",
            R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
             })",
            "invalidParams",
            "Required field 'role' missing"},
        NoRippleParamTestCaseBundle{
            "LimitNotInt",
            R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": "gg"
             })",
            "invalidParams",
            "Invalid parameters."},
        NoRippleParamTestCaseBundle{
            "LimitNegative",
            R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": -1
             })",
            "invalidParams",
            "Invalid parameters."},
        NoRippleParamTestCaseBundle{
            "LimitZero",
            R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "limit": 0
             })",
            "invalidParams",
            "Invalid parameters."},
        NoRippleParamTestCaseBundle{
            "TransactionsNotBool",
            R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "role": "gateway",
                "transactions": "gg"
             })",
            "invalidParams",
            "Invalid parameters."},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCNoRippleCheckGroup1,
    NoRippleCheckParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    NoRippleCheckParameterTest::NameGenerator{});

TEST_P(NoRippleCheckParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(std::nullopt));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account": "{}",
            "role": "gateway",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaIntIndex)
{
    auto constexpr seq = 12;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::nullopt));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account": "{}",
            "role": "gateway",
            "ledger_index": {}
        }})",
        ACCOUNT,
        seq));
    auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, LedgerNotExistViaStringIndex)
{
    auto constexpr seq = 12;
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(seq, _)).WillByDefault(Return(std::nullopt));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account": "{}",
            "role": "gateway",
            "ledger_index": "{}"
        }})",
        ACCOUNT,
        seq));
    auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, AccountNotExist)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return emtpy
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleUserDefaultRippleSetTrustLineNoRippleSet)
{
    static auto constexpr seq = 30;
    static auto constexpr expectedOutput =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "problems":
            [
                "You appear to have set your default ripple flag even though you are not a gateway. This is not recommended unless you are experimenting"
            ],
            "validated":true
        })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*rawBackendPtr, doFetchLedgerObject)
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, ripple::lsfDefaultRipple, 2, 200, 2, INDEX1, 2)
                                  .getSerializer()
                                  .peekData()));
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const line1 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    auto const line2 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "user"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleUserDefaultRippleUnsetTrustLineNoRippleUnSet)
{
    static auto constexpr seq = 30;
    static auto constexpr expectedOutput =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "problems":[
                "You should probably set the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "You should probably set the no ripple flag on your USD line to rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
            ],
            "validated":true
        })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*rawBackendPtr, doFetchLedgerObject)
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);

    auto const line2 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "user"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleGatewayDefaultRippleSetTrustLineNoRippleSet)
{
    static auto constexpr seq = 30;
    static auto constexpr expectedOutput =
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
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*rawBackendPtr, doFetchLedgerObject)
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, ripple::lsfDefaultRipple, 2, 200, 2, INDEX1, 2)
                                  .getSerializer()
                                  .peekData()));
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const line1 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    auto const line2 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleGatewayDefaultRippleUnsetTrustLineNoRippleUnset)
{
    static auto constexpr seq = 30;
    static auto constexpr expectedOutput =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "problems":
            [
                "You should immediately set your default ripple flag"
            ],
            "validated":true
        })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*rawBackendPtr, doFetchLedgerObject)
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);

    auto const line2 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathRoleGatewayDefaultRippleUnsetTrustLineNoRippleUnsetHighAccount)
{
    static auto constexpr seq = 30;
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*rawBackendPtr, doFetchLedgerObject)
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::fees().key, seq, _))
        .WillByDefault(Return(CreateFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT2, 10, ACCOUNT, 20, TXNID, 123, 0);

    auto const line2 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT2, 10, ACCOUNT, 20, TXNID, 123, 0);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway",
            "transactions": true
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("transactions").as_array().size(), 1);
        EXPECT_EQ(output->as_object().at("problems").as_array().size(), 1);
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathLimit)
{
    constexpr auto seq = 30;
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*rawBackendPtr, doFetchLedgerObject)
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, ripple::lsfDefaultRipple, 2, 200, 2, INDEX1, 2)
                                  .getSerializer()
                                  .peekData()));
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const line1 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    auto const line2 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway",
            "limit": 1
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("problems").as_array().size(), 1);
    });
}

TEST_F(RPCNoRippleCheckTest, NormalPathTransactions)
{
    constexpr auto seq = 30;
    constexpr auto transactionSeq = 123;
    const auto expectedOutput = fmt::format(
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
        transactionSeq,
        transactionSeq + 1,
        ripple::tfClearNoRipple,
        transactionSeq + 2,
        ripple::tfClearNoRipple);
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);   // min
    mockBackendPtr->updateRange(seq);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*rawBackendPtr, doFetchLedgerObject)
        .WillByDefault(
            Return(CreateAccountRootObject(ACCOUNT, 0, transactionSeq, 200, 2, INDEX1, 2).getSerializer().peekData()));
    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::fees().key, seq, _))
        .WillByDefault(Return(CreateFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    auto const line1 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    auto const line2 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    std::vector<Blob> bbs;
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway",
            "transactions": true
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOutput));
    });
}

TEST_F(RPCNoRippleCheckTest, LimitMoreThanMax)
{
    constexpr auto seq = 30;
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, seq);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // fetch account object return valid account with DefaultRippleSet flag

    ON_CALL(*rawBackendPtr, doFetchLedgerObject)
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, ripple::lsfDefaultRipple, 2, 200, 2, INDEX1, 2)
                                  .getSerializer()
                                  .peekData()));
    auto const ownerDir =
        CreateOwnerDirLedgerObject(std::vector{NoRippleCheckHandler::LIMIT_MAX + 1, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, seq, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    auto const line1 = CreateRippleStateLedgerObject(
        ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, ripple::lsfLowNoRipple);

    std::vector<Blob> bbs;
    for (auto i = 0; i < NoRippleCheckHandler::LIMIT_MAX + 1; i++)
    {
        bbs.push_back(line1.getSerializer().peekData());
    }

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}",
            "role": "gateway",
            "limit": {}
        }})",
        ACCOUNT,
        LEDGERHASH,
        NoRippleCheckHandler::LIMIT_MAX + 1));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NoRippleCheckHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("problems").as_array().size(), NoRippleCheckHandler::LIMIT_MAX);
    });
}
