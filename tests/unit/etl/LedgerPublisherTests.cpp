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

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kSeq = 30;
constexpr auto kAge = 800;
constexpr auto kAmount = 100;
constexpr auto kFee = 3;
constexpr auto kFinalBalance = 110;
constexpr auto kFinalBalancE2 = 30;

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
    // Use kAge (800) which is > MAX_LEDGER_AGE_SECONDS (600) to test skipping
    auto const dummyLedgerHeader = createLedgerHeader(kLedgerHash, kSeq, kAge);
    auto dummyState = etl::SystemState{};
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    backend_->setRange(kSeq - 1, kSeq);
    publisher.publish(dummyLedgerHeader);

    // Verify last published sequence is set immediately
    auto const seq = publisher.getLastPublishedSequence();
    ASSERT_TRUE(seq.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(seq.value(), kSeq);

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
    auto const dummyLedgerHeader = createLedgerHeader(kLedgerHash, kSeq, 0);
    auto dummyState = etl::SystemState{};
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kSeq, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSeq, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{}));

    EXPECT_CALL(
        *mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSeq - 1, kSeq), 0)
    );
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);

    backend_->setRange(kSeq - 1, kSeq);
    publisher.publish(dummyLedgerHeader);

    // Verify last published sequence is set immediately
    auto const seq = publisher.getLastPublishedSequence();
    ASSERT_TRUE(seq.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(seq.value(), kSeq);

    ctx.join();
    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderIsWritingTrue)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;
    auto const dummyLedgerHeader = createLedgerHeader(kLedgerHash, kSeq, kAge);
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    publisher.publish(dummyLedgerHeader);
    auto const seq = publisher.getLastPublishedSequence();
    ASSERT_TRUE(seq.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(seq.value(), kSeq);

    ctx.join();

    EXPECT_FALSE(backend_->fetchLedgerRange());
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderInRange)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    auto const dummyLedgerHeader = createLedgerHeader(kLedgerHash, kSeq, 0);  // age is 0
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSeq - 1, kSeq);

    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kSeq, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, kAmount, kFee, kSeq)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, kFinalBalance, kFinalBalancE2)
            .getSerializer()
            .peekData();
    t1.ledgerSequence = kSeq;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger)
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1}));

    EXPECT_CALL(
        *mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSeq - 1, kSeq), 1)
    );
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    // mock 1 transaction
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction);

    publisher.publish(dummyLedgerHeader);
    auto const seq = publisher.getLastPublishedSequence();
    ASSERT_TRUE(seq.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(seq.value(), kSeq);

    ctx.join();

    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerHeaderCloseTimeGreaterThanNow)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    auto dummyLedgerHeader = createLedgerHeader(kLedgerHash, kSeq, 0);
    auto const nowPlus10 = system_clock::now() + seconds(10);
    auto const closeTime =
        duration_cast<seconds>(nowPlus10.time_since_epoch()).count() - kRippleEpochStart;
    dummyLedgerHeader.closeTime = xrpl::NetClock::time_point{seconds{closeTime}};

    backend_->setRange(kSeq - 1, kSeq);

    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kSeq, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, kAmount, kFee, kSeq)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, kFinalBalance, kFinalBalancE2)
            .getSerializer()
            .peekData();
    t1.ledgerSequence = kSeq;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSeq, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1}));

    EXPECT_CALL(
        *mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSeq - 1, kSeq), 1)
    );
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction);

    publisher.publish(dummyLedgerHeader);
    auto const seq = publisher.getLastPublishedSequence();
    ASSERT_TRUE(seq.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(seq.value(), kSeq);

    ctx.join();

    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqStopIsTrue)
{
    auto dummyState = etl::SystemState{};
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    publisher.stop();
    EXPECT_FALSE(publisher.publish(kSeq, {}));
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqMaxAttempt)
{
    auto dummyState = etl::SystemState{};
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    static constexpr auto kMaxAttempt = 2;

    LedgerRange const range{.minSequence = kSeq - 1, .maxSequence = kSeq - 1};
    EXPECT_CALL(*backend_, hardFetchLedgerRange).Times(kMaxAttempt).WillRepeatedly(Return(range));

    EXPECT_FALSE(publisher.publish(kSeq, kMaxAttempt, std::chrono::milliseconds{1}));
}

TEST_F(ETLLedgerPublisherTest, PublishLedgerSeqStopIsFalse)
{
    auto dummyState = etl::SystemState{};
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);

    LedgerRange const range{.minSequence = kSeq, .maxSequence = kSeq};
    EXPECT_CALL(*backend_, hardFetchLedgerRange).WillOnce(Return(range));

    auto const dummyLedgerHeader = createLedgerHeader(kLedgerHash, kSeq, kAge);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kSeq, _)).WillOnce(Return(dummyLedgerHeader));

    EXPECT_TRUE(publisher.publish(kSeq, {}));
    ctx.join();
}

TEST_F(ETLLedgerPublisherTest, PublishMultipleTxInOrder)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    auto const dummyLedgerHeader = createLedgerHeader(kLedgerHash, kSeq, 0);  // age is 0
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSeq - 1, kSeq);

    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kSeq, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    // t1 index > t2 index
    TransactionAndMetadata t1;
    t1.transaction = createPaymentTransactionObject(kAccount, kAccount2, kAmount, kFee, kSeq)
                         .getSerializer()
                         .peekData();
    t1.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, kFinalBalance, kFinalBalancE2, 2)
            .getSerializer()
            .peekData();
    t1.ledgerSequence = kSeq;
    t1.date = 1;
    TransactionAndMetadata t2;
    t2.transaction = createPaymentTransactionObject(kAccount, kAccount2, kAmount, kFee, kSeq)
                         .getSerializer()
                         .peekData();
    t2.metadata =
        createPaymentTransactionMetaObject(kAccount, kAccount2, kFinalBalance, kFinalBalancE2, 1)
            .getSerializer()
            .peekData();
    t2.ledgerSequence = kSeq;
    t2.date = 2;

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSeq, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{t1, t2}));

    EXPECT_CALL(
        *mockSubscriptionManagerPtr, pubLedger(_, _, fmt::format("{}-{}", kSeq - 1, kSeq), 2)
    );
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges);

    Sequence const s;
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction(t2, _)).InSequence(s);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction(t1, _)).InSequence(s);

    publisher.publish(dummyLedgerHeader);
    auto const seq = publisher.getLastPublishedSequence();
    ASSERT_TRUE(seq.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(seq.value(), kSeq);

    ctx.join();

    EXPECT_TRUE(publisher.lastPublishAgeSeconds() <= 1);
}

TEST_F(ETLLedgerPublisherTest, PublishVeryOldLedgerShouldSkip)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    // Create a ledger header with age (800) greater than MAX_LEDGER_AGE_SECONDS (600)
    auto const dummyLedgerHeader = createLedgerHeader(kLedgerHash, kSeq, 800);
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSeq - 1, kSeq);

    EXPECT_CALL(*mockSubscriptionManagerPtr, pubLedger).Times(0);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubBookChanges).Times(0);
    EXPECT_CALL(*mockSubscriptionManagerPtr, pubTransaction).Times(0);

    publisher.publish(dummyLedgerHeader);
    auto const seq = publisher.getLastPublishedSequence();
    ASSERT_TRUE(seq.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(seq.value(), kSeq);

    ctx.join();
}

TEST_F(ETLLedgerPublisherTest, PublishMultipleLedgersInQuickSuccession)
{
    auto dummyState = etl::SystemState{};
    dummyState.isWriting = true;

    auto const dummyLedgerHeader1 = createLedgerHeader(kLedgerHash, kSeq, 0);
    auto const dummyLedgerHeader2 = createLedgerHeader(kLedgerHash, kSeq + 1, 0);
    auto publisher = impl::LedgerPublisher(ctx, backend_, mockSubscriptionManagerPtr, dummyState);
    backend_->setRange(kSeq - 1, kSeq + 1);

    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kSeq, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));
    EXPECT_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kSeq + 1, _))
        .WillOnce(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSeq, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{}));
    EXPECT_CALL(*backend_, fetchAllTransactionsInLedger(kSeq + 1, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{}));

    Sequence const s;
    EXPECT_CALL(
        *mockSubscriptionManagerPtr, pubLedger(ledgerHeaderMatcher(dummyLedgerHeader1), _, _, _)
    )
        .InSequence(s);
    EXPECT_CALL(
        *mockSubscriptionManagerPtr, pubBookChanges(ledgerHeaderMatcher(dummyLedgerHeader1), _)
    )
        .InSequence(s);
    EXPECT_CALL(
        *mockSubscriptionManagerPtr, pubLedger(ledgerHeaderMatcher(dummyLedgerHeader2), _, _, _)
    )
        .InSequence(s);
    EXPECT_CALL(
        *mockSubscriptionManagerPtr, pubBookChanges(ledgerHeaderMatcher(dummyLedgerHeader2), _)
    )
        .InSequence(s);

    // Publish two ledgers in quick succession
    publisher.publish(dummyLedgerHeader1);
    publisher.publish(dummyLedgerHeader2);

    auto const seq = publisher.getLastPublishedSequence();
    ASSERT_TRUE(seq.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(seq.value(), kSeq + 1);

    ctx.join();
}
