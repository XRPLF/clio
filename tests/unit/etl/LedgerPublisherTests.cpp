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

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kSEQ = 30;
constexpr auto kAGE = 800;

}  // namespace

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
    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, kAGE);
    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);
    publisher.publish(dummyLedgerHeader);
    EXPECT_CALL(mockCache, isDisabled).WillOnce(Return(true));
    EXPECT_CALL(*backend_, fetchLedgerDiff(kSEQ, _)).Times(0);

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    ctx_.run();
    EXPECT_TRUE(backend_->fetchLedgerRange());
    EXPECT_EQ(backend_->fetchLedgerRange().value().minSequence, kSEQ);
    EXPECT_EQ(backend_->fetchLedgerRange().value().maxSequence, kSEQ);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderIsWritingFalseAndCacheEnabled)
{
    SystemState dummyState;
    dummyState.isWriting = false;
    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, kAGE);
    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);
    publisher.publish(dummyLedgerHeader);
    EXPECT_CALL(mockCache, isDisabled).WillOnce(Return(false));
    EXPECT_CALL(*backend_, fetchLedgerDiff(kSEQ, _)).Times(1);

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    EXPECT_CALL(mockCache, updateImp);

    ctx_.run();
    EXPECT_TRUE(backend_->fetchLedgerRange());
    EXPECT_EQ(backend_->fetchLedgerRange().value().minSequence, kSEQ);
    EXPECT_EQ(backend_->fetchLedgerRange().value().maxSequence, kSEQ);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderIsWritingTrue)
{
    SystemState dummyState;
    dummyState.isWriting = true;
    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, kAGE);
    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);
    publisher.publish(dummyLedgerHeader);

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    ctx_.run();
    EXPECT_FALSE(backend_->fetchLedgerRange());
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderInRange)
{
    SystemState dummyState;
    dummyState.isWriting = true;

    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, 0);  // age is 0
    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSEQ - 1, kSEQ);

    publisher.publish(dummyLedgerHeader);

    // mock fetch fee
    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kACCOUNT, kACCOUNT2, 100, 3, kSEQ).getSerializer().peekData();
    t1.metadata = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = kSEQ;

    // mock fetch transactions
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger).WillOnce(Return(std::vector<TransactionAndMetadata>{t1}));

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSEQ - 1, kSEQ), 1));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    // mock 1 transaction
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction);

    ctx_.run();
    // last publish time should be set
    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderCloseTimeGreaterThanNow)
{
    SystemState dummyState;
    dummyState.isWriting = true;

    ripple::LedgerHeader dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, 0);
    auto const nowPlus10 = system_clock::now() + seconds(10);
    auto const closeTime = duration_cast<seconds>(nowPlus10.time_since_epoch()).count() - kRIPPLE_EPOCH_START;
    dummyLedgerHeader.closeTime = ripple::NetClock::time_point{seconds{closeTime}};

    backend_->setRange(kSEQ - 1, kSEQ);

    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);
    publisher.publish(dummyLedgerHeader);

    // mock fetch fee
    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kACCOUNT, kACCOUNT2, 100, 3, kSEQ).getSerializer().peekData();
    t1.metadata = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = kSEQ;

    // mock fetch transactions
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSEQ, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1}));

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSEQ - 1, kSEQ), 1));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    // mock 1 transaction
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction);

    ctx_.run();
    // last publish time should be set
    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqStopIsTrue)
{
    SystemState dummyState;
    dummyState.isStopping = true;
    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);
    EXPECT_FALSE(publisher.publish(kSEQ, {}));
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqMaxAttampt)
{
    SystemState dummyState;
    dummyState.isStopping = false;
    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);

    static constexpr auto kMAX_ATTEMPT = 2;

    LedgerRange const range{.minSequence = kSEQ - 1, .maxSequence = kSEQ - 1};
    EXPECT_CALL(*backend_, hardFetchLedgerRange).Times(kMAX_ATTEMPT).WillRepeatedly(Return(range));

    EXPECT_FALSE(publisher.publish(kSEQ, kMAX_ATTEMPT, std::chrono::milliseconds{1}));
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqStopIsFalse)
{
    SystemState dummyState;
    dummyState.isStopping = false;
    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);

    LedgerRange const range{.minSequence = kSEQ, .maxSequence = kSEQ};
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(Return(range));

    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, kAGE);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kSEQ, _)).WillOnce(Return(dummyLedgerHeader));
    EXPECT_CALL(mockCache, isDisabled).WillOnce(Return(false));
    EXPECT_CALL(*backend_, fetchLedgerDiff(kSEQ, _)).WillOnce(Return(std::vector<LedgerObject>{}));
    EXPECT_CALL(mockCache, updateImp);

    EXPECT_TRUE(publisher.publish(kSEQ, {}));
    ctx_.run();
}

TEST_F(ETLLedgerPublisherTest, PublishMultipleTxInOrder)
{
    SystemState dummyState;
    dummyState.isWriting = true;

    auto const dummyLedgerHeader = createLedgerHeader(kLEDGER_HASH, kSEQ, 0);  // age is 0
    impl::LedgerPublisher publisher(ctx_, backend_, mockCache, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSEQ - 1, kSEQ);

    publisher.publish(dummyLedgerHeader);

    // mock fetch fee
    EXPECT_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, kSEQ, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    // t1 index > t2 index
    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kACCOUNT, kACCOUNT2, 100, 3, kSEQ).getSerializer().peekData();
    t1.metadata = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, 110, 30, 2).getSerializer().peekData();
    t1.ledgerSequence = kSEQ;
    t1.date = 1;
    TransactionAndMetadata t2;
    t2.transaction = createPaymentTransactionObject(kACCOUNT, kACCOUNT2, 100, 3, kSEQ).getSerializer().peekData();
    t2.metadata = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, 110, 30, 1).getSerializer().peekData();
    t2.ledgerSequence = kSEQ;
    t2.date = 2;

    // mock fetch transactions
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSEQ, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1, t2}));

    // setLastPublishedSequence not in strand, should verify before run
    EXPECT_TRUE(publisher.getLastPublishedSequence());
    EXPECT_EQ(publisher.getLastPublishedSequence().value(), kSEQ);

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSEQ - 1, kSEQ), 2));
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    // should call pubTransaction t2 first (greater tx index)
    Sequence const s;
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction(t2, _)).InSequence(s);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction(t1, _)).InSequence(s);

    ctx_.run();
    // last publish time should be set
    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}
