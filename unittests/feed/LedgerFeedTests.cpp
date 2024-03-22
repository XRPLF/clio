//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "feed/FeedTestUtil.hpp"
#include "feed/impl/LedgerFeed.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/protocol/Fees.h>

constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

using namespace feed::impl;
namespace json = boost::json;
using namespace testing;

using FeedLedgerTest = FeedBaseTest<LedgerFeed>;

TEST_F(FeedLedgerTest, SubPub)
{
    backend->setRange(10, 30);
    auto const ledgerInfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(testing::Return(ledgerInfo));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(testing::Return(feeBlob));
    // check the function response
    // Information about the ledgers on hand and current fee schedule. This
    // includes the same fields as a ledger stream message, except that it omits
    // the type and txn_count fields
    constexpr static auto LedgerResponse =
        R"({
            "validated_ledgers":"10-30",
            "ledger_index":30,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":1,
            "reserve_base":3,
            "reserve_inc":2
        })";
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto res = testFeedPtr->sub(yield, backend, sessionPtr);
        // check the response
        EXPECT_EQ(res, json::parse(LedgerResponse));
    });
    ctx.run();
    EXPECT_EQ(testFeedPtr->count(), 1);

    constexpr static auto ledgerPub =
        R"({
            "type":"ledgerClosed",
            "ledger_index":31,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":0,
            "reserve_base":10,
            "reserve_inc":0,
            "validated_ledgers":"10-31",
            "txn_count":8
        })";

    // test publish
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(ledgerPub))).Times(1);
    auto const ledgerinfo2 = CreateLedgerInfo(LEDGERHASH, 31);
    auto fee2 = ripple::Fees();
    fee2.reserve = 10;
    testFeedPtr->pub(ledgerinfo2, fee2, "10-31", 8);
    ctx.restart();
    ctx.run();

    // test unsub, after unsub the send should not be called
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    EXPECT_CALL(*mockSessionPtr, send(_)).Times(0);
    testFeedPtr->pub(ledgerinfo2, fee2, "10-31", 8);
    ctx.restart();
    ctx.run();
}

TEST_F(FeedLedgerTest, AutoDisconnect)
{
    backend->setRange(10, 30);
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    EXPECT_CALL(*backend, fetchLedgerBySequence).WillOnce(testing::Return(ledgerinfo));

    auto const feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend, doFetchLedgerObject).WillOnce(testing::Return(feeBlob));
    constexpr static auto LedgerResponse =
        R"({
            "validated_ledgers":"10-30",
            "ledger_index":30,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":1,
            "reserve_base":3,
            "reserve_inc":2
        })";
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto res = testFeedPtr->sub(yield, backend, sessionPtr);
        // check the response
        EXPECT_EQ(res, json::parse(LedgerResponse));
    });
    ctx.run();
    EXPECT_EQ(testFeedPtr->count(), 1);
    EXPECT_CALL(*mockSessionPtr, send(_)).Times(0);

    sessionPtr.reset();
    EXPECT_EQ(testFeedPtr->count(), 0);

    auto const ledgerinfo2 = CreateLedgerInfo(LEDGERHASH, 31);
    auto fee2 = ripple::Fees();
    fee2.reserve = 10;
    // no error
    testFeedPtr->pub(ledgerinfo2, fee2, "10-31", 8);
    ctx.restart();
    ctx.run();
}
