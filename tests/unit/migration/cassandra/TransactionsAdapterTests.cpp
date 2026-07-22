#include "migration/cassandra/impl/TransactionsAdapter.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <optional>
#include <utility>

using namespace migration::cassandra::impl;

namespace {

constexpr auto kAccount1 = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
constexpr auto kAccount2 = "rK1EX542EgA9m948JrJRaEzwLVEhqWvnr9";
constexpr std::uint32_t kLedgerSeq = 100;

// onRowRead never touches the backend pointer; the deserialization logic is pure.
xrpl::Blob
serializedPaymentTx()
{
    return createPaymentTransactionObject(kAccount1, kAccount2, 1, 10, 1)
        .getSerializer()
        .peekData();
}

xrpl::Blob
serializedPaymentMeta()
{
    return createPaymentTransactionMetaObject(kAccount1, kAccount2, 100, 200)
        .getSerializer()
        .peekData();
}

// Build a row as onRowRead expects it: {hash, date, ledgerSeq, metadata, transaction}.
TableTransactionsDesc::Row
makeRow(xrpl::Blob metaBlob, xrpl::Blob txBlob)
{
    return TableTransactionsDesc::Row{
        xrpl::uint256{}, std::uint64_t{0}, kLedgerSeq, std::move(metaBlob), std::move(txBlob)
    };
}

}  // namespace

// A well-formed transaction row is deserialized and forwarded to the callback with the
// reconstructed STTx and TxMeta.
TEST(TransactionsAdapterTest, ValidRowInvokesCallback)
{
    auto const txBlob = serializedPaymentTx();
    std::optional<xrpl::STTx> seenTx;
    std::optional<xrpl::TxMeta> seenMeta;

    bool decodeFailed = false;
    TransactionsAdapter adapter{
        nullptr,
        [&](xrpl::STTx const& sttx, xrpl::TxMeta const& txMeta) {
            seenTx.emplace(sttx);
            seenMeta.emplace(txMeta);
        },
        [&] { decodeFailed = true; }
    };

    adapter.onRowRead(makeRow(serializedPaymentMeta(), txBlob));

    EXPECT_FALSE(decodeFailed);
    ASSERT_TRUE(seenTx.has_value());
    ASSERT_TRUE(seenMeta.has_value());

    xrpl::STTx const expectedTx{xrpl::SerialIter{txBlob.data(), txBlob.size()}};
    EXPECT_EQ(seenTx->getTransactionID(), expectedTx.getTransactionID());
    EXPECT_EQ(seenMeta->getTxID(), expectedTx.getTransactionID());
    EXPECT_EQ(seenMeta->getLgrSeq(), kLedgerSeq);
}

// A transaction blob that fails to deserialize does not invoke the transaction callback and instead
// reports a decode failure to the owner, which decides the policy (count, abort).
TEST(TransactionsAdapterTest, CorruptTransactionBlobReportsDecodeFailure)
{
    bool called = false;
    bool decodeFailed = false;
    TransactionsAdapter adapter{
        nullptr,
        [&](xrpl::STTx const&, xrpl::TxMeta const&) { called = true; },
        [&] { decodeFailed = true; }
    };

    adapter.onRowRead(makeRow(serializedPaymentMeta(), xrpl::Blob{0xFF, 0xFF, 0xFF, 0xFF}));

    EXPECT_FALSE(called);
    EXPECT_TRUE(decodeFailed);
}

// A metadata blob that fails to deserialize (valid transaction, garbage meta) also reports a decode
// failure without invoking the transaction callback.
TEST(TransactionsAdapterTest, CorruptMetadataBlobReportsDecodeFailure)
{
    bool called = false;
    bool decodeFailed = false;
    TransactionsAdapter adapter{
        nullptr,
        [&](xrpl::STTx const&, xrpl::TxMeta const&) { called = true; },
        [&] { decodeFailed = true; }
    };

    adapter.onRowRead(makeRow(xrpl::Blob{0xFF, 0xFF, 0xFF, 0xFF}, serializedPaymentTx()));

    EXPECT_FALSE(called);
    EXPECT_TRUE(decodeFailed);
}
