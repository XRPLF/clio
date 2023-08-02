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
#include <rpc/handlers/AccountChannels.h>
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

class RPCAccountChannelsHandlerTest : public HandlerBaseTest
{
};

TEST_F(RPCAccountChannelsHandlerTest, LimitNotInt)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": "t"
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, LimitNagetive)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": -1
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, LimitZero)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 0
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
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

TEST_F(RPCAccountChannelsHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
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

TEST_F(RPCAccountChannelsHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
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

TEST_F(RPCAccountChannelsHandlerTest, MarkerNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
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
TEST_F(RPCAccountChannelsHandlerTest, InvalidMarker)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
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
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
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
TEST_F(RPCAccountChannelsHandlerTest, AccountInvalidFormat)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const input = json::parse(R"({ 
            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp"
        })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "accountMalformed");
    });
}

// error case: account invalid format
TEST_F(RPCAccountChannelsHandlerTest, AccountNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const input = json::parse(R"({ 
            "account": 12
        })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerHash)
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
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerStringIndex)
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
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerIntIndex)
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
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerHash2)
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
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // no need to check from db,call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "ledger_index": "31"
        }})",
        ACCOUNT));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case account not exist
TEST_F(RPCAccountChannelsHandlerTest, NonExistAccount)
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
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

// normal case when only provide account
TEST_F(RPCAccountChannelsHandlerTest, DefaultParameterTest)
{
    constexpr static auto correctOutput = R"({
        "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index":30,
        "validated":true,
        "limit":200,
        "channels":[
            {
                "channel_id":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "amount":"100",
                "balance":"10",
                "settle_delay":32,
                "public_key":"aBMxWrnPUnvwZPfsmTyVizxEGsGheAu3Tsn6oPRgyjgvd2NggFxz",
                "public_key_hex":"020000000000000000000000000000000000000000000000000000000000000000"
            },
            {
                "channel_id":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "amount":"100",
                "balance":"10",
                "settle_delay":32,
                "public_key":"aBMxWrnPUnvwZPfsmTyVizxEGsGheAu3Tsn6oPRgyjgvd2NggFxz",
                "public_key_hex":"020000000000000000000000000000000000000000000000000000000000000000"
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

    // return owner index containing 2 indexes
    ripple::STObject ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}, INDEX1);

    ON_CALL(*rawBackendPtr, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}"
        }})",
        ACCOUNT));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output);
    });
}

// normal case : limit is used
TEST_F(RPCAccountChannelsHandlerTest, UseLimit)
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
        ripple::STObject channel = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
        bbs.push_back(channel.getSerializer().peekData());
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
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}",
                "limit": 20
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        EXPECT_EQ((*output).as_object().at("channels").as_array().size(), 20);
        EXPECT_THAT((*output).as_object().at("marker").as_string().c_str(), EndsWith(",0"));
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 9
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit?
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{mockBackendPtr}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 401
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit?
    });
}

// normal case : destination is used
TEST_F(RPCAccountChannelsHandlerTest, UseDestination)
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

    // 10 pay channel to ACCOUNT2
    auto repetitions = 10;
    while (repetitions--)
    {
        indexes.push_back(ripple::uint256{INDEX1});
        ripple::STObject channel = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
        bbs.push_back(channel.getSerializer().peekData());
    }

    // 20 pay channel to ACCOUNT3
    repetitions = 20;
    while (repetitions--)
    {
        indexes.push_back(ripple::uint256{INDEX1});
        ripple::STObject channel = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT3, 100, 10, 32, TXNID, 28);
        bbs.push_back(channel.getSerializer().peekData());
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
            "destination_account":"{}"
        }})",
        ACCOUNT,
        ACCOUNT3));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output).as_object().at("channels").as_array().size(), 20);
    });
}

// normal case : but the lines is emtpy
TEST_F(RPCAccountChannelsHandlerTest, EmptyChannel)
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
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output).as_object().at("channels").as_array().size(), 0);
    });
}

// Return expiration cancel_offer source_tag destination_tag when available
TEST_F(RPCAccountChannelsHandlerTest, OptionalResponseField)
{
    constexpr static auto correctOutput = R"({
        "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_index":30,
        "validated":true,
        "limit":200,
        "channels":[
            {
                "channel_id":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "amount":"100",
                "balance":"10",
                "settle_delay":32,
                "public_key":"aBMxWrnPUnvwZPfsmTyVizxEGsGheAu3Tsn6oPRgyjgvd2NggFxz",
                "public_key_hex":"020000000000000000000000000000000000000000000000000000000000000000",
                "expiration": 100,
                "cancel_after": 200,
                "source_tag": 300,
                "destination_tag": 400
            },
            {
                "channel_id":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "destination_account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "amount":"100",
                "balance":"10",
                "settle_delay":32,
                "public_key":"aBMxWrnPUnvwZPfsmTyVizxEGsGheAu3Tsn6oPRgyjgvd2NggFxz",
                "public_key_hex":"020000000000000000000000000000000000000000000000000000000000000000",
                "expiration": 100,
                "cancel_after": 200,
                "source_tag": 300,
                "destination_tag": 400
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

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    channel1.setFieldU32(ripple::sfExpiration, 100);
    channel1.setFieldU32(ripple::sfCancelAfter, 200);
    channel1.setFieldU32(ripple::sfSourceTag, 300);
    channel1.setFieldU32(ripple::sfDestinationTag, 400);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}"
        }})",
        ACCOUNT));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output);
    });
}

// normal case : test marker output correct
TEST_F(RPCAccountChannelsHandlerTest, MarkerOutput)
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
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
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
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = CreateOwnerDirLedgerObject(indexes, INDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, nextPage);
    // first page 's next page is 99
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
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output).as_object().at("marker").as_string().c_str(), fmt::format("{},{}", INDEX1, nextPage));
        EXPECT_EQ((*output).as_object().at("channels").as_array().size(), 15);
    });
}

// normal case : handler marker correctly
TEST_F(RPCAccountChannelsHandlerTest, MarkerInput)
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
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    int objectsCount = limit;
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0)
    {
        // return owner index
        indexes.push_back(ripple::uint256{INDEX1});
        bbs.push_back(channel1.getSerializer().peekData());
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
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE((*output).as_object().if_contains("marker") == nullptr);
        // the first item is the marker itself, so the result will have limit-1
        // items
        EXPECT_EQ((*output).as_object().at("channels").as_array().size(), limit - 1);
    });
}

TEST_F(RPCAccountChannelsHandlerTest, LimitLessThanMin)
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

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "limit": {}
        }})",
        ACCOUNT,
        AccountChannelsHandler::LIMIT_MIN - 1));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output).as_object().at("channels").as_array().size(), 2);
        EXPECT_EQ((*output).as_object().at("limit").as_uint64(), AccountChannelsHandler::LIMIT_MIN);
    });
}

TEST_F(RPCAccountChannelsHandlerTest, LimitMoreThanMax)
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

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject channel1 = CreatePaymentChannelLedgerObject(ACCOUNT, ACCOUNT2, 100, 10, 32, TXNID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "limit": {}
        }})",
        ACCOUNT,
        AccountChannelsHandler::LIMIT_MAX + 1));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output).as_object().at("channels").as_array().size(), 2);
        EXPECT_EQ((*output).as_object().at("limit").as_uint64(), AccountChannelsHandler::LIMIT_MAX);
    });
}
