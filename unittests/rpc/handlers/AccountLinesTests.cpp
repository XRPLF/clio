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
#include <rpc/handlers/AccountLines.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto ACCOUNT3 = "rB9BMzh27F3Q6a5FtGPDayQoCCEdiRdqcK";
constexpr static auto INDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr static auto TXNID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";

class RPCAccountLinesHandlerTest : public HandlerBaseTest
{
};

// TODO: a lot of the tests are copy-pasted from AccountChannelsTest
// because the logic is mostly the same but currently implemented in
// a separate handler class. We should eventually use some sort of
// base class or common component to these `account_*` rpcs.

TEST_F(RPCAccountLinesHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 10,
                "ledger_hash": "xxx"
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCAccountLinesHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{
                "account": "{}", 
                "limit": 10,
                "ledger_hash": 123
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCAccountLinesHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 10,
                "ledger_index": "notvalidated"
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

TEST_F(RPCAccountLinesHandlerTest, MarkerNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "marker": 9
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerNotString");
    });
}

// error case : invalid marker
// marker format is composed of a comma separated index and start hint. The
// former will be read as hex, and the latter using boost lexical cast.
TEST_F(RPCAccountLinesHandlerTest, InvalidMarker)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}",
                "marker": "123invalid"
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Malformed cursor.");
    });
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "marker": 401
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

// error case: account invalid format, length is incorrect
TEST_F(RPCAccountLinesHandlerTest, AccountInvalidFormat)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp"
            })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

// error case: account invalid format
TEST_F(RPCAccountLinesHandlerTest, AccountNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(
            R"({ 
                "account": 12
            })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAccountLinesHandlerTest, PeerInvalidFormat)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "peer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp"
            })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAccountLinesHandlerTest, PeerNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "peer": 12
            })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitNotInt)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "limit": "t"
            })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitNagetive)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "limit": -1
            })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitZero)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "limit": 0
            })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

// error case ledger non exist via hash
TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerHash)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // mock fetchLedgerByHash return empty
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // mock fetchLedgerBySequence return empty
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "ledger_index": "4"
        }})",
        ACCOUNT));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // mock fetchLedgerBySequence return empty
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "ledger_index": 4
        }})",
        ACCOUNT));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerHash2)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // mock fetchLedgerByHash return ledger but seq is 31 > 30
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 31);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // no need to check from db, call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "ledger_index": "31"
        }})",
        ACCOUNT));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case account not exist
TEST_F(RPCAccountLinesHandlerTest, NonExistAccount)
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
            "ledger_hash": "{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

// normal case when only provide account
TEST_F(RPCAccountLinesHandlerTest, DefaultParameterTest)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index containing 2 indexes
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    // return two trust lines
    std::vector<Blob> bbs;
    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
    auto const line2 =
        CreateRippleStateLedgerObject(ACCOUNT2, "USD", ACCOUNT, 10, ACCOUNT2, 100, ACCOUNT, 200, TXNID, 123);
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([this](auto yield) {
        auto const input = json::parse(fmt::format(
            R"({{
                "account": "{}"
            }})",
            ACCOUNT));
        auto const correctOutput =
            R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": 30,
                "validated": true,
                "limit": 200,
                "lines": [
                    {
                        "account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "balance": "10",
                        "currency": "USD",
                        "limit": "100",
                        "limit_peer": "200",
                        "quality_in": 0,
                        "quality_out": 0,
                        "no_ripple": false,
                        "no_ripple_peer": false
                    },
                    {
                        "account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "balance": "-10",
                        "currency": "USD",
                        "limit": "200",
                        "limit_peer": "100",
                        "quality_in": 0,
                        "quality_out": 0,
                        "no_ripple": false,
                        "no_ripple_peer": false
                    }
                ]
            })";

        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output);
    });
}

// normal case : limit is used
TEST_F(RPCAccountLinesHandlerTest, UseLimit)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(3);
    // fetch account object return something
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;

    auto repetitions = 50;
    while (repetitions--)
    {
        indexes.push_back(ripple::uint256{INDEX1});
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    // it should not appear in return marker,marker is the current page
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(7);

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(3);

    runSpawn([this](auto yield) {
        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}",
                "limit": 20
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        EXPECT_EQ((*output).as_object().at("lines").as_array().size(), 20);
        EXPECT_THAT((*output).as_object().at("marker").as_string().c_str(), EndsWith(",0"));
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 9
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit somehow?
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 401
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit somehow?
    });
}

// normal case : destination is used
TEST_F(RPCAccountLinesHandlerTest, UseDestination)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;

    // 10 lines to ACCOUNT2
    auto repetitions = 10;
    while (repetitions--)
    {
        indexes.push_back(ripple::uint256{INDEX1});
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    // 20 lines to ACCOUNT3
    repetitions = 20;
    while (repetitions--)
    {
        indexes.push_back(ripple::uint256{INDEX1});
        auto const line =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT3, 10, ACCOUNT, 100, ACCOUNT3, 200, TXNID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "limit": 30,       
            "peer": "{}"
        }})",
        ACCOUNT,
        ACCOUNT3));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output).as_object().at("lines").as_array().size(), 20);
    });
}

// normal case : but the lines is emtpy
TEST_F(RPCAccountLinesHandlerTest, EmptyChannel)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject({}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}"
        }})",
        ACCOUNT));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output).as_object().at("lines").as_array().size(), 0);
    });
}

TEST_F(RPCAccountLinesHandlerTest, OptionalResponseField)
{
    constexpr static auto correctOutput = R"({
        "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index": 30,
        "validated": true,
        "limit": 200,
        "lines": [
            {
                "account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "balance": "10",
                "currency": "USD",
                "limit": "100",
                "limit_peer": "200",
                "quality_in": 0,
                "quality_out": 0,
                "no_ripple": false,
                "no_ripple_peer": true,
                "peer_authorized": true,
                "freeze_peer": true
            },
            {
                "account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "balance": "20",
                "currency": "USD",
                "limit": "200",
                "limit_peer": "400",
                "quality_in": 0,
                "quality_out": 0,
                "no_ripple": true,
                "no_ripple_peer": false,
                "authorized": true,
                "freeze": true
            }
        ]
    })";
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};

    // return a non empty account
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    // return few trust lines
    std::vector<Blob> bbs;
    auto line1 = CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 0);
    line1.setFlag(ripple::lsfHighAuth);
    line1.setFlag(ripple::lsfHighNoRipple);
    line1.setFlag(ripple::lsfHighFreeze);
    bbs.push_back(line1.getSerializer().peekData());

    auto line2 = CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 20, ACCOUNT, 200, ACCOUNT2, 400, TXNID, 0);
    line2.setFlag(ripple::lsfLowAuth);
    line2.setFlag(ripple::lsfLowNoRipple);
    line2.setFlag(ripple::lsfLowFreeze);
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}"
        }})",
        ACCOUNT));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output);
    });
}

// normal case : test marker output correct
TEST_F(RPCAccountLinesHandlerTest, MarkerOutput)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto ownerDirKk = ripple::keylet::ownerDir(account).key;
    constexpr static auto nextPage = 99;
    constexpr static auto limit = 15;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), nextPage).key;
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    auto line = CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 0);

    // owner dir contains 10 indexes
    int objectsCount = 10;
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0)
    {
        // return owner index
        indexes.push_back(ripple::uint256{INDEX1});
        objectsCount--;
    }
    // return 15 objects
    objectsCount = 15;
    while (objectsCount != 0)
    {
        bbs.push_back(line.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, nextPage);
    // first page's next page is 99
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ripple::STObject ownerDir2 = CreateOwnerDirLedgerObject(indexes, INDEX1);
    // second page's next page is 0
    ownerDir2.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "limit": {}
        }})",
        ACCOUNT,
        limit));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output).as_object().at("marker").as_string().c_str(), fmt::format("{},{}", INDEX1, nextPage));
        EXPECT_EQ((*output).as_object().at("lines").as_array().size(), 15);
    });
}

// normal case : handler marker correctly
TEST_F(RPCAccountLinesHandlerTest, MarkerInput)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    constexpr static auto nextPage = 99;
    constexpr static auto limit = 15;
    auto ownerDirKk = ripple::keylet::page(ripple::keylet::ownerDir(account), nextPage).key;
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    auto const line =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 0);
    int objectsCount = limit;
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0)
    {
        // return owner index
        indexes.push_back(ripple::uint256{INDEX1});
        bbs.push_back(line.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "limit": {},
            "marker": "{},{}"
        }})",
        ACCOUNT,
        limit,
        INDEX1,
        nextPage));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE((*output).as_object().if_contains("marker") == nullptr);
        // the first item is the marker itself, so the result will have limit-1
        // items
        EXPECT_EQ((*output).as_object().at("lines").as_array().size(), limit - 1);
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitLessThanMin)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index containing 2 indexes
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    // return two trust lines
    std::vector<Blob> bbs;
    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
    auto const line2 =
        CreateRippleStateLedgerObject(ACCOUNT2, "USD", ACCOUNT, 10, ACCOUNT2, 100, ACCOUNT, 200, TXNID, 123);
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([this](auto yield) {
        auto const input = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "limit": {}
            }})",
            ACCOUNT,
            AccountLinesHandler::LIMIT_MIN - 1));
        auto const correctOutput = fmt::format(
            R"({{
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": 30,
                "validated": true,
                "limit": {},
                "lines": [
                    {{
                        "account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "balance": "10",
                        "currency": "USD",
                        "limit": "100",
                        "limit_peer": "200",
                        "quality_in": 0,
                        "quality_out": 0,
                        "no_ripple": false,
                        "no_ripple_peer": false
                    }},
                    {{
                        "account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "balance": "-10",
                        "currency": "USD",
                        "limit": "200",
                        "limit_peer": "100",
                        "quality_in": 0,
                        "quality_out": 0,
                        "no_ripple": false,
                        "no_ripple_peer": false
                    }}
                ]
            }})",
            AccountLinesHandler::LIMIT_MIN);

        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output);
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitMoreThanMax)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = GetAccountIDWithString(ACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index containing 2 indexes
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    // return two trust lines
    std::vector<Blob> bbs;
    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ACCOUNT2, 10, ACCOUNT, 100, ACCOUNT2, 200, TXNID, 123);
    auto const line2 =
        CreateRippleStateLedgerObject(ACCOUNT2, "USD", ACCOUNT, 10, ACCOUNT2, 100, ACCOUNT, 200, TXNID, 123);
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    runSpawn([this](auto yield) {
        auto const input = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "limit": {}
            }})",
            ACCOUNT,
            AccountLinesHandler::LIMIT_MAX + 1));
        auto const correctOutput = fmt::format(
            R"({{
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": 30,
                "validated": true,
                "limit": {},
                "lines": [
                    {{
                        "account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "balance": "10",
                        "currency": "USD",
                        "limit": "100",
                        "limit_peer": "200",
                        "quality_in": 0,
                        "quality_out": 0,
                        "no_ripple": false,
                        "no_ripple_peer": false
                    }},
                    {{
                        "account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "balance": "-10",
                        "currency": "USD",
                        "limit": "200",
                        "limit_peer": "100",
                        "quality_in": 0,
                        "quality_out": 0,
                        "no_ripple": false,
                        "no_ripple_peer": false
                    }}
                ]
            }})",
            AccountLinesHandler::LIMIT_MAX);

        auto handler = AnyHandler{AccountLinesHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output);
    });
}
