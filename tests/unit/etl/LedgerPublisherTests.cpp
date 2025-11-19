//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "data/DBHelpers.hpp"
#include "data/Types.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/LedgerPublisher.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/TestObject.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <chrono>
#include <optional>
#include <vector>

using namespace testing;
using namespace etl;
using namespace data;
using namespace std::chrono;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kSEQ = 30;
constexpr auto kAGE = 800;
constexpr auto kAMOUNT = 100;
constexpr auto kFEE = 3;
constexpr auto kFINAL_BALANCE = 110;
constexpr auto kFINAL_BALANCE2 = 30;

MATCHER_P(ledgerHeaderMatcher, expectedHeader, "Headers match")
{
    return arg.seq == expectedHeader.seq && arg.hash == expectedHeader.hash &&
        arg.closeTime == expectedHeader.closeTime;
}

}  // namespace

struct ETLLedgerPublisherTest : util::prometheus::WithPrometheus, MockBackendTestStrict {
    util::config::ClioConfigDefinition cfg{{}};
    StrictMockSubscriptionManagerSharedPtr mockSubscriptionManagerPtr;
    util::async::CoroExecutionContext ctx;
};

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderSkipDueToAge)
{
    // Use kAGE (800) which is > MAX_LEDGER_AGE_SECONDS (600) to test skipping
    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, kAGE);
    auto dummyState = etl::SystemState{};
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    backend_->setRange(kSEQ - 1, kSEQ);
    publisher.publish(dummyLedgerHeader);

    // Verify last published sequence is set immediately
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    // Since age > MAX_LEDGER_AGE_SECONDS, these should not be called
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(0);
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).Times(0);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger).Times(0);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges).Times(0);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction).Times(0);

    ctx.join();
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderWithinAgeLimit)
{
    // Use age 0 which is < MAX_LEDGER_AGE_SECONDS to ensure publishing happens
    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, 0);
    auto dummyState = etl::SystemState{};
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSEQ, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{}));

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSEQ - 1, kSEQ), 0));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);

    backend_->setRange(kSEQ - 1, kSEQ);
    publisher.publish(dummyLedgerHeader);

    // Verify last published sequence is set immediately
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    ctx.join();
    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderIsWritingTrue)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;
    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, kAGE);
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    publisher.publish(dummyLedgerHeader);
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    ctx.join();

    EXPECT_FALSE(backend_->fetchLedgerRange());
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderInRange)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, 0);  // age is 0
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSEQ - 1, kSEQ);

    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    TransactionAndMetadata t1;
    t1.transaction =
        createPaymentTransactionObject(kACCOUNT, kACCOUNT2, kAMOUNT, kFEE, kSEQ).getSerializer().peekData();
    t1.metadata = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, kFINAL_BALANCE, kFINAL_BALANCE2)
                      .getSerializer()
                      .peekData();
    t1.ledgerSequence = kSEQ;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).WillOnce(Return(std::vector<TransactionAndMetadata>{t1}));

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSEQ - 1, kSEQ), 1));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    // mock 1 transaction
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction);

    publisher.publish(dummyLedgerHeader);
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    ctx.join();

    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderCloseTimeGreaterThanNow)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    auto dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, 0);
    auto const nowPlus10 = system_clock::now() + seconds(10);
    auto const closeTime = duration_cast<seconds>(nowPlus10.time_since_epoch()).count() - kRIPPLE_EPOCH_START;
    dummyLedgerHeader.closeTime = ripple::NetClock::time_point{seconds{closeTime}};

    backend_->setRange(kSEQ - 1, kSEQ);

    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    TransactionAndMetadata t1;
    t1.transaction =
        createPaymentTransactionObject(kACCOUNT, kACCOUNT2, kAMOUNT, kFEE, kSEQ).getSerializer().peekData();
    t1.metadata = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, kFINAL_BALANCE, kFINAL_BALANCE2)
                      .getSerializer()
                      .peekData();
    t1.ledgerSequence = kSEQ;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSEQ, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1}));

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSEQ - 1, kSEQ), 1));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction);

    publisher.publish(dummyLedgerHeader);
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    ctx.join();

    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqStopIsTrue)
{
    auto dummyState = etl::SystemState{};
    dummyState.isStopping = true;
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    EXPECT_FALSE(publisher.publish(kSEQ, {}));
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqMaxAttempt)
{
    auto dummyState = etl::SystemState{};
    dummyState.isStopping = false;
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    static constexpr auto kMAX_ATTEMPT = 2;

    LedgerRange const range{.minSequence = kSEQ - 1, .maxSequence = kSEQ - 1};
    EXPECT_CALL(*backend_, hardFetchLedgerRange).Times(kMAX_ATTEMPT).WillRepeatedly(Return(range));

    EXPECT_FALSE(publisher.publish(kSEQ, kMAX_ATTEMPT, std::chrono::milliseconds{1}));
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqStopIsFalse)
{
    auto dummyState = etl::SystemState{};
    dummyState.isStopping = false;
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    LedgerRange const range{.minSequence = kSEQ, .maxSequence = kSEQ};
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(Return(range));

    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, kAGE);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kSEQ, _)).WillOnce(Return(dummyLedgerHeader));

    EXPECT_TRUE(publisher.publish(kSEQ, {}));
    ctx.join();
}

TEST_F(ETLLedgerPublisherTest, PublishMultipleTxInOrder)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, 0);  // age is 0
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSEQ - 1, kSEQ);

    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    // t1 index > t2 index
    TransactionAndMetadata t1;
    t1.transaction =
        createPaymentTransactionObject(kACCOUNT, kACCOUNT2, kAMOUNT, kFEE, kSEQ).getSerializer().peekData();
    t1.metadata = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, kFINAL_BALANCE, kFINAL_BALANCE2, 2)
                      .getSerializer()
                      .peekData();
    t1.ledgerSequence = kSEQ;
    t1.date = 1;
    TransactionAndMetadata t2;
    t2.transaction =
        createPaymentTransactionObject(kACCOUNT, kACCOUNT2, kAMOUNT, kFEE, kSEQ).getSerializer().peekData();
    t2.metadata = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, kFINAL_BALANCE, kFINAL_BALANCE2, 1)
                      .getSerializer()
                      .peekData();
    t2.ledgerSequence = kSEQ;
    t2.date = 2;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSEQ, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1, t2}));

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSEQ - 1, kSEQ), 2));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);

    Sequence const s;
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction(t2, _)).InSequence(s);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction(t1, _)).InSequence(s);

    publisher.publish(dummyLedgerHeader);
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    ctx.join();

    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishVeryOldLedgerShouldSkip)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    // Create a ledger header with age (800) greater than MAX_LEDGER_AGE_SECONDS (600)
    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, 800);
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSEQ - 1, kSEQ);

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger).Times(0);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges).Times(0);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction).Times(0);

    publisher.publish(dummyLedgerHeader);
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    ctx.join();
}

TEST_F(ETLLedgerPublisherTest, PublishMultipleLedgersInQuickSuccession)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    auto const dummyLedgerHeader1 = createLedgerHeader(kLEDGER_HASH, kSEQ, 0);
    auto const dummyLedgerHeader2 = createLedgerHeader(kLEDGER_HASH, kSEQ + 1, 0);
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSEQ - 1, kSEQ + 1);

    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ + 1, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSEQ, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{}));
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSEQ + 1, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{}));

    Sequence const s;
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(ledgerHeaderMatcher(dummyLedgerHeader1), _, _, _)).InSequence(s);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges(ledgerHeaderMatcher(dummyLedgerHeader1), _)).InSequence(s);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(ledgerHeaderMatcher(dummyLedgerHeader2), _, _, _)).InSequence(s);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges(ledgerHeaderMatcher(dummyLedgerHeader2), _)).InSequence(s);

    // Publish two ledgers in quick succession
    publisher.publish(dummyLedgerHeader1);
    publisher.publish(dummyLedgerHeader2);

    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ + 1);

    ctx.join();
}
