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
#include "rpc/handlers/AccountChannels.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kACCOUNT3 = "rB9BMzh27F3Q6a5FtGPDayQoCCEdiRdqcK";
constexpr auto kINDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kTXN_ID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";

}  // namespace

struct RPCAccountChannelsHandlerTest : HandlerBaseTest {
    RPCAccountChannelsHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

TEST_F(RPCAccountChannelsHandlerTest, LimitNotInt)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": "t"
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, LimitNagetive)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": -1
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, LimitZero)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 0
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 10,
                "ledger_hash": "xxx"
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{
                "account": "{}", 
                "limit": 10,
                "ledger_hash": 123
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 10,
                "ledger_index": "notvalidated"
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, MarkerNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "marker": 9
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
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
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}",
                "marker": "123invalid"
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Malformed cursor.");
    });
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "marker": 401
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

// error case: account invalid format, length is incorrect
TEST_F(RPCAccountChannelsHandlerTest, AccountInvalidFormat)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(R"({ 
            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp"
        })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "accountMalformed");
    });
}

// error case: account invalid format
TEST_F(RPCAccountChannelsHandlerTest, AccountNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(R"({ 
            "account": 12
        })");
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerStringIndex)
{
    // mock fetchLedgerBySequence return empty
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "ledger_index": "4"
        }})",
        kACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerIntIndex)
{
    // mock fetchLedgerBySequence return empty
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
                "account": "{}",
                "ledger_index": 4
        }})",
        kACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerHash2)
{
    // mock fetchLedgerByHash return ledger but seq is 31 > 30
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 31);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCAccountChannelsHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    // no need to check from db,call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "ledger_index": "31"
        }})",
        kACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case account not exist
TEST_F(RPCAccountChannelsHandlerTest, NonExistAccount)
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
            "ledger_hash": "{}"
        }})",
        kACCOUNT,
        kLEDGER_HASH
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

// normal case when only provide account
TEST_F(RPCAccountChannelsHandlerTest, DefaultParameterTest)
{
    static constexpr auto kCORRECT_OUTPUT = R"({
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

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index containing 2 indexes
    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}"
        }})",
        kACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(kCORRECT_OUTPUT), *output.result);
    });
}

// normal case : limit is used
TEST_F(RPCAccountChannelsHandlerTest, UseLimit)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(3);
    // fetch account object return something
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;

    auto repetitions = 50;
    while ((repetitions--) != 0) {
        indexes.emplace_back(kINDEX1);
        ripple::STObject const channel =
            createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
        bbs.push_back(channel.getSerializer().peekData());
    }
    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    // it should not appear in return marker,marker is the current page
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(7);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(3);

    runSpawn([this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}",
                "limit": 20
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        EXPECT_EQ((*output.result).as_object().at("channels").as_array().size(), 20);
        EXPECT_THAT(boost::json::value_to<std::string>((*output.result).as_object().at("marker")), EndsWith(",0"));
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 9
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit?
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountChannelsHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 401
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit?
    });
}

// normal case : destination is used
TEST_F(RPCAccountChannelsHandlerTest, UseDestination)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;

    // 10 pay channel to Account2
    auto repetitions = 10;
    while ((repetitions--) != 0) {
        indexes.emplace_back(kINDEX1);
        ripple::STObject const channel =
            createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
        bbs.push_back(channel.getSerializer().peekData());
    }

    // 20 pay channel to Account3
    repetitions = 20;
    while ((repetitions--) != 0) {
        indexes.emplace_back(kINDEX1);
        ripple::STObject const channel =
            createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT3, 100, 10, 32, kTXN_ID, 28);
        bbs.push_back(channel.getSerializer().peekData());
    }

    ripple::STObject const ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "limit": 30,       
            "destination_account":"{}"
        }})",
        kACCOUNT,
        kACCOUNT3
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("channels").as_array().size(), 20);
    });
}

// normal case : but the lines is emtpy
TEST_F(RPCAccountChannelsHandlerTest, EmptyChannel)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index
    ripple::STObject const ownerDir = createOwnerDirLedgerObject({}, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}"
        }})",
        kACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("channels").as_array().size(), 0);
    });
}

// Return expiration cancel_offer source_tag destination_tag when available
TEST_F(RPCAccountChannelsHandlerTest, OptionalResponseField)
{
    static constexpr auto kCORRECT_OUTPUT = R"({
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

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index
    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    channel1.setFieldU32(ripple::sfExpiration, 100);
    channel1.setFieldU32(ripple::sfCancelAfter, 200);
    channel1.setFieldU32(ripple::sfSourceTag, 300);
    channel1.setFieldU32(ripple::sfDestinationTag, 400);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}"
        }})",
        kACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(kCORRECT_OUTPUT), *output.result);
    });
}

// normal case : test marker output correct
TEST_F(RPCAccountChannelsHandlerTest, MarkerOutput)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto ownerDirKk = ripple::keylet::ownerDir(account).key;
    static constexpr auto kNEXT_PAGE = 99;
    static constexpr auto kLIMIT = 15;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    // owner dir contains 10 indexes
    int objectsCount = 10;
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        objectsCount--;
    }
    // return 15 objects
    objectsCount = 15;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, kNEXT_PAGE);
    // first page 's next page is 99
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ripple::STObject ownerDir2 = createOwnerDirLedgerObject(indexes, kINDEX1);
    // second page's next page is 0
    ownerDir2.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "limit": {}
        }})",
        kACCOUNT,
        kLIMIT
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            boost::json::value_to<std::string>((*output.result).as_object().at("marker")),
            fmt::format("{},{}", kINDEX1, kNEXT_PAGE)
        );
        EXPECT_EQ((*output.result).as_object().at("channels").as_array().size(), 15);
    });
}

// normal case : handler marker correctly
TEST_F(RPCAccountChannelsHandlerTest, MarkerInput)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    static constexpr auto kNEXT_PAGE = 99;
    static constexpr auto kLIMIT = 15;
    auto ownerDirKk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto fake = Blob{'f', 'a', 'k', 'e'};
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    int objectsCount = kLIMIT;
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}",
            "limit": {},
            "marker": "{},{}"
        }})",
        kACCOUNT,
        kLIMIT,
        kINDEX1,
        kNEXT_PAGE
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE((*output.result).as_object().if_contains("marker") == nullptr);
        // the first item is the marker itself, so the result will have limit-1
        // items
        EXPECT_EQ((*output.result).as_object().at("channels").as_array().size(), kLIMIT - 1);
    });
}

TEST_F(RPCAccountChannelsHandlerTest, LimitLessThanMin)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index containing 2 indexes
    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "limit": {}
        }})",
        kACCOUNT,
        AccountChannelsHandler::kLIMIT_MIN - 1
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("channels").as_array().size(), 2);
        EXPECT_EQ((*output.result).as_object().at("limit").as_uint64(), AccountChannelsHandler::kLIMIT_MIN);
    });
}

TEST_F(RPCAccountChannelsHandlerTest, LimitMoreThanMax)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // fetch account object return something
    auto account = getAccountIdWithString(kACCOUNT);
    auto accountKk = ripple::keylet::account(account).key;
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    auto fake = Blob{'f', 'a', 'k', 'e'};
    // return a non empty account
    ON_CALL(*backend_, doFetchLedgerObject(accountKk, testing::_, testing::_)).WillByDefault(Return(fake));

    // return owner index containing 2 indexes
    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "account": "{}",
            "limit": {}
        }})",
        kACCOUNT,
        AccountChannelsHandler::kLIMIT_MAX + 1
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountChannelsHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("channels").as_array().size(), 2);
        EXPECT_EQ((*output.result).as_object().at("limit").as_uint64(), AccountChannelsHandler::kLIMIT_MAX);
    });
}
