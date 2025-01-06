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
#include "rpc/handlers/AccountLines.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
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

struct RPCAccountLinesHandlerTest : HandlerBaseTest {
    RPCAccountLinesHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

// TODO: a lot of the tests are copy-pasted from AccountChannelsTest
// because the logic is mostly the same but currently implemented in
// a separate handler class. We should eventually use some sort of
// base class or common component to these `account_*` rpcs.

TEST_F(RPCAccountLinesHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
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

TEST_F(RPCAccountLinesHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
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

TEST_F(RPCAccountLinesHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
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

TEST_F(RPCAccountLinesHandlerTest, MarkerNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
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
TEST_F(RPCAccountLinesHandlerTest, InvalidMarker)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
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
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
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
TEST_F(RPCAccountLinesHandlerTest, AccountInvalidFormat)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp"
            })"
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

// error case: account invalid format
TEST_F(RPCAccountLinesHandlerTest, AccountNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(
            R"({ 
                "account": 12
            })"
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAccountLinesHandlerTest, PeerInvalidFormat)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "peer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jp"
            })"
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAccountLinesHandlerTest, PeerNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "peer": 12
            })"
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Account malformed.");
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitNotInt)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "limit": "t"
            })"
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitNagetive)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "limit": -1
            })"
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitZero)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(
            R"({ 
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "limit": 0
            })"
        );
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

// error case ledger non exist via hash
TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerHash)
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
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerStringIndex)
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
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerIntIndex)
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
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerHash2)
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
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCAccountLinesHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    // no need to check from db, call fetchLedgerBySequence 0 time
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
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case account not exist
TEST_F(RPCAccountLinesHandlerTest, NonExistAccount)
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
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

// normal case when only provide account
TEST_F(RPCAccountLinesHandlerTest, DefaultParameterTest)
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

    // return two trust lines
    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
    auto const line2 = createRippleStateLedgerObject("USD", kACCOUNT, 10, kACCOUNT2, 100, kACCOUNT, 200, kTXN_ID, 123);
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([this](auto yield) {
        auto const input = json::parse(fmt::format(
            R"({{
                "account": "{}"
            }})",
            kACCOUNT
        ));
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

        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

// normal case : limit is used
TEST_F(RPCAccountLinesHandlerTest, UseLimit)
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
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
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
        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}",
                "limit": 20
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        EXPECT_EQ((*output.result).as_object().at("lines").as_array().size(), 20);
        EXPECT_THAT(boost::json::value_to<std::string>((*output.result).as_object().at("marker")), EndsWith(",0"));
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 9
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit somehow?
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{AccountLinesHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "account": "{}", 
                "limit": 401
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit somehow?
    });
}

// normal case : destination is used
TEST_F(RPCAccountLinesHandlerTest, UseDestination)
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

    // 10 lines to Account2
    auto repetitions = 10;
    while ((repetitions--) != 0) {
        indexes.emplace_back(kINDEX1);
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
    }

    // 20 lines to Account3
    repetitions = 20;
    while ((repetitions--) != 0) {
        indexes.emplace_back(kINDEX1);
        auto const line =
            createRippleStateLedgerObject("USD", kACCOUNT3, 10, kACCOUNT, 100, kACCOUNT3, 200, kTXN_ID, 123);
        bbs.push_back(line.getSerializer().peekData());
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
            "peer": "{}"
        }})",
        kACCOUNT,
        kACCOUNT3
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("lines").as_array().size(), 20);
    });
}

// normal case : but the lines is emtpy
TEST_F(RPCAccountLinesHandlerTest, EmptyChannel)
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
        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ((*output.result).as_object().at("lines").as_array().size(), 0);
    });
}

TEST_F(RPCAccountLinesHandlerTest, OptionalResponseField)
{
    static constexpr auto kCORRECT_OUTPUT = R"({
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

    // return few trust lines
    std::vector<Blob> bbs;
    auto line1 = createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 0);
    line1.setFlag(ripple::lsfHighAuth);
    line1.setFlag(ripple::lsfHighNoRipple);
    line1.setFlag(ripple::lsfHighFreeze);
    bbs.push_back(line1.getSerializer().peekData());

    auto line2 = createRippleStateLedgerObject("USD", kACCOUNT2, 20, kACCOUNT, 200, kACCOUNT2, 400, kTXN_ID, 0);
    line2.setFlag(ripple::lsfLowAuth);
    line2.setFlag(ripple::lsfLowNoRipple);
    line2.setFlag(ripple::lsfLowFreeze);
    bbs.push_back(line2.getSerializer().peekData());

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "account": "{}"
        }})",
        kACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(kCORRECT_OUTPUT), *output.result);
    });
}

// normal case : test marker output correct
TEST_F(RPCAccountLinesHandlerTest, MarkerOutput)
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
    auto line = createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 0);

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
        bbs.push_back(line.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, kNEXT_PAGE);
    // first page's next page is 99
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
        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            boost::json::value_to<std::string>((*output.result).as_object().at("marker")),
            fmt::format("{},{}", kINDEX1, kNEXT_PAGE)
        );
        EXPECT_EQ((*output.result).as_object().at("lines").as_array().size(), 15);
    });
}

// normal case : handler marker correctly
TEST_F(RPCAccountLinesHandlerTest, MarkerInput)
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
    auto const line = createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 0);
    int objectsCount = kLIMIT;
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        bbs.push_back(line.getSerializer().peekData());
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
        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE((*output.result).as_object().if_contains("marker") == nullptr);
        // the first item is the marker itself, so the result will have limit-1
        // items
        EXPECT_EQ((*output.result).as_object().at("lines").as_array().size(), kLIMIT - 1);
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitLessThanMin)
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

    // return two trust lines
    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
    auto const line2 = createRippleStateLedgerObject("USD", kACCOUNT, 10, kACCOUNT2, 100, kACCOUNT, 200, kTXN_ID, 123);
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([this](auto yield) {
        auto const input = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "limit": {}
            }})",
            kACCOUNT,
            AccountLinesHandler::kLIMIT_MIN - 1
        ));
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
            AccountLinesHandler::kLIMIT_MIN
        );

        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

TEST_F(RPCAccountLinesHandlerTest, LimitMoreThanMax)
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

    // return two trust lines
    std::vector<Blob> bbs;
    auto const line1 = createRippleStateLedgerObject("USD", kACCOUNT2, 10, kACCOUNT, 100, kACCOUNT2, 200, kTXN_ID, 123);
    auto const line2 = createRippleStateLedgerObject("USD", kACCOUNT, 10, kACCOUNT2, 100, kACCOUNT, 200, kTXN_ID, 123);
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(line2.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    runSpawn([this](auto yield) {
        auto const input = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "limit": {}
            }})",
            kACCOUNT,
            AccountLinesHandler::kLIMIT_MAX + 1
        ));
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
            AccountLinesHandler::kLIMIT_MAX
        );

        auto handler = AnyHandler{AccountLinesHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(correctOutput), *output.result);
    });
}

TEST(RPCAccountLinesHandlerSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"account", "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"},
        {"peer", "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"},
        {"ignore_default", false},
        {"ledger_hash", "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652"},
        {"limit", 200},
        {"ledger_index", 30},
        {"marker", "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun,0"},
        {"ledger", 123},
        {"strict", true},
        {"peer_index", 456}
    };
    auto const spec = AccountLinesHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    ASSERT_TRUE(warnings[0].is_object());
    auto const& warning = warnings[0].as_object();
    ASSERT_TRUE(warning.contains("id"));
    ASSERT_TRUE(warning.contains("message"));
    EXPECT_EQ(warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::WarnRpcDeprecated));
    for (auto const& field : {"ledger", "peer_index"}) {
        EXPECT_NE(
            warning.at("message").as_string().find(fmt::format("Field '{}' is deprecated.", field)), std::string::npos
        ) << warning;
    }
}
