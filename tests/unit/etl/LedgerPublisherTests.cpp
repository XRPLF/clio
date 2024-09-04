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

#include "data/DBHelpers.hpp"
#include "data/Types.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/LedgerPublisher.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockCache.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/TestObject.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <chrono>
#include <vector>

using namespace testing;
using namespace etl;
using namespace std::chrono;

static auto constexpr ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
static auto constexpr ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
static auto constexpr LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
static auto constexpr SEQ = 30;
static auto constexpr AGE = 800;

struct ETLLedgerPublisherTest : util::prometheus::WithPrometheus, MockBackendTestStrict, SyncAsioContextTest {
    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
    }

    void
    TearDown() override
    {
        SyncAsioContextTest::TearDown();
    }
    util::config::ClioConfigDefinition cfg{{}};
    MockCache mockCache;
    StrictMockSubscriptionManagerSharedPtr mockSubscriptionManagerPtr;
};

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderIsWritingFalseAndCacheDisabled)
{
    SystemState dummyState;
    dummyState.isWriting = false;
    auto const dummyLedgerHeader = CreateLedgerHeader(LEDGERHASH, SEQ, AGE);
    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);
    publisher.publish(dummyLedgerHeader);
    EXPECT_CALL(mockCache, isDisabled).WillOnce(Return(true));
    EXPECT_CALL(*backend, fetchLedgerDiff(SEQ, _)).Times(0);

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), SEQ);

    ctx.run();
    EXPECT_TRUE(backend->fetchLedgerRange());
    EXPECT_EQ(backend->fetchLedgerRange().value().minSequence, SEQ);
    EXPECT_EQ(backend->fetchLedgerRange().value().maxSequence, SEQ);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderIsWritingFalseAndCacheEnabled)
{
    SystemState dummyState;
    dummyState.isWriting = false;
    auto const dummyLedgerHeader = CreateLedgerHeader(LEDGERHASH, SEQ, AGE);
    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);
    publisher.publish(dummyLedgerHeader);
    EXPECT_CALL(mockCache, isDisabled).WillOnce(Return(false));
    EXPECT_CALL(*backend, fetchLedgerDiff(SEQ, _)).Times(1);

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), SEQ);

    EXPECT_CALL(mockCache, updateImp);

    ctx.run();
    EXPECT_TRUE(backend->fetchLedgerRange());
    EXPECT_EQ(backend->fetchLedgerRange().value().minSequence, SEQ);
    EXPECT_EQ(backend->fetchLedgerRange().value().maxSequence, SEQ);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderIsWritingTrue)
{
    SystemState dummyState;
    dummyState.isWriting = true;
    auto const dummyLedgerHeader = CreateLedgerHeader(LEDGERHASH, SEQ, AGE);
    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);
    publisher.publish(dummyLedgerHeader);

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), SEQ);

    ctx.run();
    EXPECT_FALSE(backend->fetchLedgerRange());
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderInRange)
{
    SystemState dummyState;
    dummyState.isWriting = true;

    auto const dummyLedgerHeader = CreateLedgerHeader(LEDGERHASH, SEQ, 0);  // age is 0
    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);
    backend->setRange(SEQ - 1, SEQ);

    publisher.publish(dummyLedgerHeader);

    // mock fetch fee
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::fees().key, SEQ, _))
        .WillOnce(Return(CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    TransactionAndMetadata t1;
    t1.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 100, 3, SEQ).getSerializer().peekData();
    t1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = SEQ;

    // mock fetch transactions
    EXPECT_CALL(*backend, fetchAllTransactionsInLedger).WillOnce(Return(std::vector<TransactionAndMetadata>{t1}));

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), SEQ);

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", SEQ - 1, SEQ), 1));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    // mock 1 transaction
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction);

    ctx.run();
    // last publish time should be set
    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderCloseTimeGreaterThanNow)
{
    SystemState dummyState;
    dummyState.isWriting = true;

    ripple::LedgerHeader dummyLedgerHeader = CreateLedgerHeader(LEDGERHASH, SEQ, 0);
    auto const nowPlus10 = system_clock::now() + seconds(10);
    auto const closeTime = duration_cast<seconds>(nowPlus10.time_since_epoch()).count() - rippleEpochStart;
    dummyLedgerHeader.closeTime = ripple::NetClock::time_point{seconds{closeTime}};

    backend->setRange(SEQ - 1, SEQ);

    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);
    publisher.publish(dummyLedgerHeader);

    // mock fetch fee
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::fees().key, SEQ, _))
        .WillOnce(Return(CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    TransactionAndMetadata t1;
    t1.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 100, 3, SEQ).getSerializer().peekData();
    t1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = SEQ;

    // mock fetch transactions
    EXPECT_CALL(*backend, fetchAllTransactionsInLedger(SEQ, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1}));

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), SEQ);

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", SEQ - 1, SEQ), 1));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    // mock 1 transaction
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction);

    ctx.run();
    // last publish time should be set
    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqStopIsTrue)
{
    SystemState dummyState;
    dummyState.isStopping = true;
    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);
    EXPECT_FALSE(publisher.publish(SEQ, {}));
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqMaxAttampt)
{
    SystemState dummyState;
    dummyState.isStopping = false;
    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);

    static auto constexpr MAX_ATTEMPT = 2;

    LedgerRange const range{.minSequence = SEQ - 1, .maxSequence = SEQ - 1};
    EXPECT_CALL(*backend, hardFetchLedgerRange).Times(MAX_ATTEMPT).WillRepeatedly(Return(range));

    EXPECT_FALSE(publisher.publish(SEQ, MAX_ATTEMPT, std::chrono::milliseconds{1}));
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqStopIsFalse)
{
    SystemState dummyState;
    dummyState.isStopping = false;
    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);

    LedgerRange const range{.minSequence = SEQ, .maxSequence = SEQ};
    EXPECT_CALL(*backend, hardFetchLedgerRange).WillOnce(Return(range));

    auto const dummyLedgerHeader = CreateLedgerHeader(LEDGERHASH, SEQ, AGE);
    EXPECT_CALL(*backend, fetchLedgerBySequence(SEQ, _)).WillOnce(Return(dummyLedgerHeader));
    EXPECT_CALL(mockCache, isDisabled).WillOnce(Return(false));
    EXPECT_CALL(*backend, fetchLedgerDiff(SEQ, _)).WillOnce(Return(std::vector<LedgerObject>{}));
    EXPECT_CALL(mockCache, updateImp);

    EXPECT_TRUE(publisher.publish(SEQ, {}));
    ctx.run();
}

TEST_F(ETLLedgerPublisherTest, PublishMultipleTxInOrder)
{
    SystemState dummyState;
    dummyState.isWriting = true;

    auto const dummyLedgerHeader = CreateLedgerHeader(LEDGERHASH, SEQ, 0);  // age is 0
    impl::LedgerPublisher publisher(ctx, backend, mockCache, mockSubscriptionManagerPtr, dummyState);
    backend->setRange(SEQ - 1, SEQ);

    publisher.publish(dummyLedgerHeader);

    // mock fetch fee
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::fees().key, SEQ, _))
        .WillOnce(Return(CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    // t1 index > t2 index
    TransactionAndMetadata t1;
    t1.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 100, 3, SEQ).getSerializer().peekData();
    t1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30, 2).getSerializer().peekData();
    t1.ledgerSequence = SEQ;
    t1.date = 1;
    TransactionAndMetadata t2;
    t2.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 100, 3, SEQ).getSerializer().peekData();
    t2.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30, 1).getSerializer().peekData();
    t2.ledgerSequence = SEQ;
    t2.date = 2;

    // mock fetch transactions
    EXPECT_CALL(*backend, fetchAllTransactionsInLedger(SEQ, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1, t2}));

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), SEQ);

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", SEQ - 1, SEQ), 2));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    // should call pubTransaction t2 first (greater tx index)
    Sequence const s;
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction(t2, _)).InSequence(s);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction(t1, _)).InSequence(s);

    ctx.run();
    // last publish time should be set
    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}
