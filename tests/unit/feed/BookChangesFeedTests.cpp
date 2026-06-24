#include "data/Types.hpp"
#include "feed/FeedTestUtil.hpp"
#include "feed/impl/BookChangesFeed.hpp"
#include "feed/impl/ForwardFeed.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/STObject.h>

#include <vector>

using namespace feed::impl;
using namespace data;

namespace {

constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kAccount1 = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kCurrency = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr auto kIssuer = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";

}  // namespace

using FeedBookChangeTest = FeedBaseTest<BookChangesFeed>;

TEST_F(FeedBookChangeTest, Pub)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 32);
    auto transactions = std::vector<TransactionAndMetadata>{};
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    xrpl::STObject const metaObj = createMetaDataForBookChange(kCurrency, kIssuer, 22, 1, 3, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();
    transactions.push_back(trans1);

    static constexpr auto kBookChangePublish =
        R"JSON({
            "type": "bookChanges",
            "ledger_index": 32,
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time": 0,
            "changes": [
                {
                    "currency_a": "XRP_drops",
                    "currency_b": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD/0158415500000000C1F76FF6ECB0BAC600000000",
                    "volume_a": "2",
                    "volume_b": "2",
                    "high": "-1",
                    "low": "-1",
                    "open": "-1",
                    "close": "-1"
                }
            ]
        })JSON";

    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kBookChangePublish))).Times(1);
    testFeedPtr->pub(ledgerHeader, transactions);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->pub(ledgerHeader, transactions);
}
