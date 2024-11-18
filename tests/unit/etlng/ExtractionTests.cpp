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
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSeqUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "data/DBHelpers.hpp"
#include "data/Types.hpp"
#include "etl/LedgerFetcherInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/impl/Extraction.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/LoggerFixtures.hpp"

#include <gmock/gmock.h>
#include <google/protobuf/repeated_ptr_field.h>
#include <gtest/gtest.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace {
constinit auto const Seq = 30;
}  // namespace

struct ExtractionTests : NoLoggerFixture {};

TEST_F(ExtractionTests, ModType)
{
    using namespace etlng::impl;
    using ModType = etlng::model::Object::ModType;

    EXPECT_EQ(extractModType(PBObjType::MODIFIED), ModType::Modified);
    EXPECT_EQ(extractModType(PBObjType::CREATED), ModType::Created);
    EXPECT_EQ(extractModType(PBObjType::DELETED), ModType::Deleted);
    EXPECT_EQ(extractModType(PBObjType::UNSPECIFIED), ModType::Unspecified);
}

TEST_F(ExtractionTests, OneTransaction)
{
    using namespace etlng::impl;

    auto expected = util::CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER);

    auto original = org::xrpl::rpc::v1::TransactionAndMetadata();
    auto [metaRaw, txRaw] = util::CreateNftTxAndMetaBlobs();
    original.set_transaction_blob(txRaw);
    original.set_metadata_blob(metaRaw);

    auto res = extractTx(original, Seq);
    EXPECT_EQ(res.meta.getLgrSeq(), Seq);
    EXPECT_EQ(res.meta.getLgrSeq(), expected.meta.getLgrSeq());
    EXPECT_EQ(res.meta.getTxID(), expected.meta.getTxID());
    EXPECT_EQ(res.sttx.getTxnType(), expected.sttx.getTxnType());
}

TEST_F(ExtractionTests, MultipleTransactions)
{
    using namespace etlng::impl;

    auto expected = util::CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER);

    auto original = org::xrpl::rpc::v1::TransactionAndMetadata();
    auto [metaRaw, txRaw] = util::CreateNftTxAndMetaBlobs();
    original.set_transaction_blob(txRaw);
    original.set_metadata_blob(metaRaw);

    auto list = org::xrpl::rpc::v1::TransactionAndMetadataList();
    for (auto i = 0; i < 10; ++i) {
        auto* p = list.add_transactions();
        *p = original;
    }

    auto res = extractTxs(list.transactions(), Seq);
    EXPECT_EQ(res.size(), 10);

    for (auto const& tx : res) {
        EXPECT_EQ(tx.meta.getLgrSeq(), Seq);
        EXPECT_EQ(tx.meta.getLgrSeq(), expected.meta.getLgrSeq());
        EXPECT_EQ(tx.meta.getTxID(), expected.meta.getTxID());
        EXPECT_EQ(tx.sttx.getTxnType(), expected.sttx.getTxnType());
    }
}

TEST_F(ExtractionTests, OneObject)
{
    using namespace etlng::impl;

    auto expected = util::CreateObject();
    auto original = org::xrpl::rpc::v1::RawLedgerObject();
    original.set_data(expected.dataRaw);
    original.set_key(expected.keyRaw);

    auto res = extractObj(original);
    EXPECT_EQ(ripple::strHex(res.key), ripple::strHex(expected.keyRaw));
    EXPECT_EQ(ripple::strHex(res.data), ripple::strHex(expected.dataRaw));
    EXPECT_EQ(res.predecessor, uint256ToString(data::lastKey));
    EXPECT_EQ(res.successor, uint256ToString(data::firstKey));
    EXPECT_EQ(res.type, expected.type);
}

TEST_F(ExtractionTests, OneObjectWithSuccessorAndPredecessor)
{
    using namespace etlng::impl;

    auto expected = util::CreateObject();
    auto original = org::xrpl::rpc::v1::RawLedgerObject();
    original.set_data(expected.dataRaw);
    original.set_key(expected.keyRaw);
    original.set_predecessor(expected.predecessor);
    original.set_successor(expected.successor);

    auto res = extractObj(original);
    EXPECT_EQ(ripple::strHex(res.key), ripple::strHex(expected.keyRaw));
    EXPECT_EQ(ripple::strHex(res.data), ripple::strHex(expected.dataRaw));
    EXPECT_EQ(res.predecessor, expected.predecessor);
    EXPECT_EQ(res.successor, expected.successor);
    EXPECT_EQ(res.type, expected.type);
}

TEST_F(ExtractionTests, MultipleObjects)
{
    using namespace etlng::impl;

    auto expected = util::CreateObject();
    auto original = org::xrpl::rpc::v1::RawLedgerObject();
    original.set_data(expected.dataRaw);
    original.set_key(expected.keyRaw);

    auto list = org::xrpl::rpc::v1::RawLedgerObjects();
    for (auto i = 0; i < 10; ++i) {
        auto* p = list.add_objects();
        *p = original;
    }

    auto res = extractObjs(list.objects());
    EXPECT_EQ(res.size(), 10);

    for (auto const& obj : res) {
        EXPECT_EQ(ripple::strHex(obj.key), ripple::strHex(expected.keyRaw));
        EXPECT_EQ(ripple::strHex(obj.data), ripple::strHex(expected.dataRaw));
        EXPECT_EQ(obj.predecessor, uint256ToString(data::lastKey));
        EXPECT_EQ(obj.successor, uint256ToString(data::firstKey));
        EXPECT_EQ(obj.type, expected.type);
    }
}

TEST_F(ExtractionTests, OneSuccessor)
{
    using namespace etlng::impl;

    auto expected = util::CreateSuccessor();
    auto original = org::xrpl::rpc::v1::BookSuccessor();
    original.set_first_book(expected.firstBook);
    original.set_book_base(expected.bookBase);

    auto res = extractSuccessor(original);
    EXPECT_EQ(ripple::strHex(res.firstBook), ripple::strHex(expected.firstBook));
    EXPECT_EQ(ripple::strHex(res.bookBase), ripple::strHex(expected.bookBase));
}

TEST_F(ExtractionTests, MultipleSuccessors)
{
    using namespace etlng::impl;

    auto expected = util::CreateSuccessor();
    auto original = org::xrpl::rpc::v1::BookSuccessor();
    original.set_first_book(expected.firstBook);
    original.set_book_base(expected.bookBase);

    auto data = PBLedgerResponseType();
    data.set_object_neighbors_included(true);
    for (auto i = 0; i < 10; ++i) {
        auto* el = data.mutable_book_successors()->Add();
        *el = original;
    }

    auto res = maybeExtractSuccessors(data);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->size(), 10);

    for (auto const& successor : res.value()) {
        EXPECT_EQ(successor.firstBook, expected.firstBook);
        EXPECT_EQ(successor.bookBase, expected.bookBase);
    }
}

TEST_F(ExtractionTests, SuccessorsWithNoNeighborsIncluded)
{
    using namespace etlng::impl;

    auto data = PBLedgerResponseType();
    data.set_object_neighbors_included(false);

    auto res = maybeExtractSuccessors(data);
    ASSERT_FALSE(res.has_value());
}

struct ExtractionDeathTest : NoLoggerFixture {};

TEST_F(ExtractionDeathTest, InvalidModTypeAsserts)
{
    using namespace etlng::impl;

    EXPECT_DEATH(
        {
            [[maybe_unused]] auto _ = extractModType(
                PBModType::
                    RawLedgerObject_ModificationType_RawLedgerObject_ModificationType_INT_MIN_SENTINEL_DO_NOT_USE_
            );
        },
        ".*"
    );
}

struct MockFetcher : etl::LedgerFetcherInterface {
    MOCK_METHOD(std::optional<GetLedgerResponseType>, fetchData, (uint32_t), (override));
    MOCK_METHOD(std::optional<GetLedgerResponseType>, fetchDataAndDiff, (uint32_t), (override));
};

struct ExtractorTests : ExtractionTests {
    std::shared_ptr<MockFetcher> fetcher = std::make_shared<MockFetcher>();
    etlng::impl::Extractor extractor{fetcher};
};

TEST_F(ExtractorTests, ExtractLedgerWithDiffNoResult)
{
    EXPECT_CALL(*fetcher, fetchDataAndDiff(Seq)).WillOnce(testing::Return(std::nullopt));
    auto res = extractor.extractLedgerWithDiff(Seq);
    EXPECT_FALSE(res.has_value());
}

TEST_F(ExtractorTests, ExtractLedgerOnlyNoResult)
{
    EXPECT_CALL(*fetcher, fetchData(Seq)).WillOnce(testing::Return(std::nullopt));
    auto res = extractor.extractLedgerOnly(Seq);
    EXPECT_FALSE(res.has_value());
}

TEST_F(ExtractorTests, ExtractLedgerWithDiffWithResult)
{
    auto original = util::CreateDataAndDiff();

    EXPECT_CALL(*fetcher, fetchDataAndDiff(Seq)).WillOnce(testing::Return(original));
    auto res = extractor.extractLedgerWithDiff(Seq);

    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res->objects.size(), 10);
    EXPECT_EQ(res->transactions.size(), 10);
    EXPECT_TRUE(res->successors.has_value());
    EXPECT_EQ(res->successors->size(), 10);
    EXPECT_FALSE(res->edgeKeys.has_value());  // this is set separately in ETL
}

TEST_F(ExtractorTests, ExtractLedgerOnlyWithResult)
{
    auto original = util::CreateData();

    EXPECT_CALL(*fetcher, fetchData(Seq)).WillOnce(testing::Return(original));
    auto res = extractor.extractLedgerOnly(Seq);

    EXPECT_TRUE(res.has_value());
    EXPECT_TRUE(res->objects.empty());
    EXPECT_EQ(res->transactions.size(), 10);
    EXPECT_FALSE(res->successors.has_value());
    EXPECT_FALSE(res->edgeKeys.has_value());  // this is set separately in ETL
}
