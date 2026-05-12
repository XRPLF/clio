#include "data/DBHelpers.hpp"
#include "data/Types.hpp"
#include "etl/LedgerFetcherInterface.hpp"
#include "etl/Models.hpp"
#include "etl/impl/Extraction.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockAssert.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <google/protobuf/repeated_ptr_field.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
constinit auto const kLEDGER_HASH =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kLEDGER_HASH2 =
    "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constinit auto const kSEQ = 30;
}  // namespace

struct ExtractionModelTests : virtual public ::testing::Test {};

TEST_F(ExtractionModelTests, LedgerDataCopyableAndEquatable)
{
    auto const first = etl::model::LedgerData{
        .transactions =
            {util::createTransaction(ripple::TxType::ttNFTOKEN_BURN),
             util::createTransaction(ripple::TxType::ttNFTOKEN_BURN),
             util::createTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER)},
        .objects = {util::createObject(), util::createObject(), util::createObject()},
        .successors =
            std::vector<etl::model::BookSuccessor>{{.firstBook = "first", .bookBase = "base"}},
        .edgeKeys = std::vector<std::string>{"key1", "key2"},
        .header = createLedgerHeader(kLEDGER_HASH, kSEQ, 1),
        .rawHeader = {1, 2, 3},
        .seq = kSEQ
    };

    auto const second = first;
    EXPECT_EQ(first, second);

    {
        auto third = second;
        third.transactions.clear();
        EXPECT_NE(first, third);
    }
    {
        auto third = second;
        third.objects = {util::createObject()};
        EXPECT_NE(first, third);
    }
    {
        auto third = second;
        third.successors =
            std::vector<etl::model::BookSuccessor>{{.firstBook = "second", .bookBase = "base"}};
        EXPECT_NE(first, third);
    }
    {
        auto third = second;
        third.edgeKeys = std::vector<std::string>{"key1"};
        EXPECT_NE(first, third);
    }
    {
        auto third = second;
        third.header = createLedgerHeader(
            kLEDGER_HASH2, kSEQ, 100
        );  // Using large age value to avoid flaky test
        EXPECT_NE(first, third);
    }
    {
        auto third = second;
        third.rawHeader = {2, 3, 4};
        EXPECT_NE(first, third);
    }
    {
        auto third = second;
        third.seq = kSEQ - 1;
        EXPECT_NE(first, third);
    }
}

TEST_F(ExtractionModelTests, TransactionIsEquatable)
{
    auto const tx = std::vector{util::createTransaction(ripple::TxType::ttNFTOKEN_BURN)};
    auto other = tx;
    EXPECT_EQ(tx, other);

    other.push_back(util::createTransaction(ripple::TxType::ttNFTOKEN_ACCEPT_OFFER));
    EXPECT_NE(tx, other);
}

TEST_F(ExtractionModelTests, ObjectCopyableAndEquatable)
{
    auto const obj = util::createObject();
    auto const other = obj;
    EXPECT_EQ(obj, other);

    {
        auto third = other;
        third.key = ripple::uint256{42};
        EXPECT_NE(obj, third);
    }
    {
        auto third = other;
        third.keyRaw = "key";
        EXPECT_NE(obj, third);
    }
    {
        auto third = other;
        third.data = {2, 3};
        EXPECT_NE(obj, third);
    }
    {
        auto third = other;
        third.dataRaw = "something";
        EXPECT_NE(obj, third);
    }
    {
        auto third = other;
        third.successor = "succ";
        EXPECT_NE(obj, third);
    }
    {
        auto third = other;
        third.predecessor = "pred";
        EXPECT_NE(obj, third);
    }
    {
        auto third = other;
        third.type = etl::model::Object::ModType::Deleted;
        EXPECT_NE(obj, third);
    }
}

TEST_F(ExtractionModelTests, BookSuccessorCopyableAndEquatable)
{
    auto const succ = etl::model::BookSuccessor{.firstBook = "first", .bookBase = "base"};
    auto const other = succ;
    EXPECT_EQ(succ, other);

    {
        auto third = other;
        third.bookBase = "all your base are belong to us";
        EXPECT_NE(succ, third);
    }
    {
        auto third = other;
        third.firstBook = "not the first book";
        EXPECT_NE(succ, third);
    }
}

struct ExtractionTests : public virtual ::testing::Test {};

TEST_F(ExtractionTests, ModType)
{
    using namespace etl::impl;
    using ModType = etl::model::Object::ModType;

    EXPECT_EQ(extractModType(PBObjType::MODIFIED), ModType::Modified);
    EXPECT_EQ(extractModType(PBObjType::CREATED), ModType::Created);
    EXPECT_EQ(extractModType(PBObjType::DELETED), ModType::Deleted);
    EXPECT_EQ(extractModType(PBObjType::UNSPECIFIED), ModType::Unspecified);
}

TEST_F(ExtractionTests, OneTransaction)
{
    using namespace etl::impl;

    auto expected = util::createTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER);

    auto original = org::xrpl::rpc::v1::TransactionAndMetadata();
    auto [metaRaw, txRaw] = util::createTxAndMetaBlobs();
    original.set_transaction_blob(txRaw);
    original.set_metadata_blob(metaRaw);

    auto res = extractTx(original, kSEQ);
    EXPECT_EQ(res.meta.getLgrSeq(), kSEQ);
    EXPECT_EQ(res.meta.getLgrSeq(), expected.meta.getLgrSeq());
    EXPECT_EQ(res.meta.getTxID(), expected.meta.getTxID());
    EXPECT_EQ(res.sttx.getTxnType(), expected.sttx.getTxnType());
}

TEST_F(ExtractionTests, MultipleTransactions)
{
    using namespace etl::impl;

    auto expected = util::createTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER);

    auto original = org::xrpl::rpc::v1::TransactionAndMetadata();
    auto [metaRaw, txRaw] = util::createTxAndMetaBlobs();
    original.set_transaction_blob(txRaw);
    original.set_metadata_blob(metaRaw);

    auto list = org::xrpl::rpc::v1::TransactionAndMetadataList();
    for (auto i = 0; i < 10; ++i) {
        auto* p = list.add_transactions();
        *p = original;
    }

    auto res = extractTxs(list.transactions(), kSEQ);
    EXPECT_EQ(res.size(), 10);

    for (auto const& tx : res) {
        EXPECT_EQ(tx.meta.getLgrSeq(), kSEQ);
        EXPECT_EQ(tx.meta.getLgrSeq(), expected.meta.getLgrSeq());
        EXPECT_EQ(tx.meta.getTxID(), expected.meta.getTxID());
        EXPECT_EQ(tx.sttx.getTxnType(), expected.sttx.getTxnType());
    }
}

TEST_F(ExtractionTests, OneObject)
{
    using namespace etl::impl;

    auto expected = util::createObject();
    auto original = org::xrpl::rpc::v1::RawLedgerObject();
    original.set_data(expected.dataRaw);
    original.set_key(expected.keyRaw);
    original.set_mod_type(
        org::xrpl::rpc::v1::RawLedgerObject::ModificationType::
            RawLedgerObject_ModificationType_CREATED
    );

    auto res = extractObj(original);
    EXPECT_EQ(ripple::strHex(res.key), ripple::strHex(expected.keyRaw));
    EXPECT_EQ(ripple::strHex(res.data), ripple::strHex(expected.dataRaw));
    EXPECT_EQ(res.predecessor, uint256ToString(data::kLAST_KEY));
    EXPECT_EQ(res.successor, uint256ToString(data::kFIRST_KEY));
    EXPECT_EQ(res.type, expected.type);
}

TEST_F(ExtractionTests, OneObjectWithSuccessorAndPredecessor)
{
    using namespace etl::impl;

    auto expected = util::createObject();
    auto original = org::xrpl::rpc::v1::RawLedgerObject();
    original.set_data(expected.dataRaw);
    original.set_key(expected.keyRaw);
    original.set_predecessor(expected.predecessor);
    original.set_successor(expected.successor);
    original.set_mod_type(
        org::xrpl::rpc::v1::RawLedgerObject::ModificationType::
            RawLedgerObject_ModificationType_CREATED
    );

    auto res = extractObj(original);
    EXPECT_EQ(ripple::strHex(res.key), ripple::strHex(expected.keyRaw));
    EXPECT_EQ(ripple::strHex(res.data), ripple::strHex(expected.dataRaw));
    EXPECT_EQ(res.predecessor, expected.predecessor);
    EXPECT_EQ(res.successor, expected.successor);
    EXPECT_EQ(res.type, expected.type);
}

TEST_F(ExtractionTests, MultipleObjects)
{
    using namespace etl::impl;

    auto expected = util::createObject();
    auto original = org::xrpl::rpc::v1::RawLedgerObject();
    original.set_data(expected.dataRaw);
    original.set_key(expected.keyRaw);
    original.set_mod_type(
        org::xrpl::rpc::v1::RawLedgerObject::ModificationType::
            RawLedgerObject_ModificationType_CREATED
    );

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
        EXPECT_EQ(obj.predecessor, uint256ToString(data::kLAST_KEY));
        EXPECT_EQ(obj.successor, uint256ToString(data::kFIRST_KEY));
        EXPECT_EQ(obj.type, expected.type);
    }
}

TEST_F(ExtractionTests, OneSuccessor)
{
    using namespace etl::impl;

    auto expected = util::createSuccessor();
    auto original = org::xrpl::rpc::v1::BookSuccessor();
    original.set_first_book(expected.firstBook);
    original.set_book_base(expected.bookBase);

    auto res = extractSuccessor(original);
    EXPECT_EQ(ripple::strHex(res.firstBook), ripple::strHex(expected.firstBook));
    EXPECT_EQ(ripple::strHex(res.bookBase), ripple::strHex(expected.bookBase));
}

TEST_F(ExtractionTests, MultipleSuccessors)
{
    using namespace etl::impl;

    auto expected = util::createSuccessor();
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
    EXPECT_EQ(res->size(), 10);  // NOLINT(bugprone-unchecked-optional-access)

    for (auto const& successor : res.value()) {  // NOLINT(bugprone-unchecked-optional-access)
        EXPECT_EQ(successor.firstBook, expected.firstBook);
        EXPECT_EQ(successor.bookBase, expected.bookBase);
    }
}

TEST_F(ExtractionTests, SuccessorsWithNoNeighborsIncluded)
{
    using namespace etl::impl;

    auto data = PBLedgerResponseType();
    data.set_object_neighbors_included(false);

    auto res = maybeExtractSuccessors(data);
    ASSERT_FALSE(res.has_value());
}

struct ExtractionAssertTest : common::util::WithMockAssert {};

TEST_F(ExtractionAssertTest, InvalidModTypeAsserts)
{
    using namespace etl::impl;

    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto _ = extractModType(
            PBModType::
                RawLedgerObject_ModificationType_RawLedgerObject_ModificationType_INT_MIN_SENTINEL_DO_NOT_USE_
        );
    });
}

struct MockFetcher : etl::LedgerFetcherInterface {
    MOCK_METHOD(std::optional<GetLedgerResponseType>, fetchData, (uint32_t), (override));
    MOCK_METHOD(std::optional<GetLedgerResponseType>, fetchDataAndDiff, (uint32_t), (override));
};

struct ExtractorTests : ExtractionTests {
    std::shared_ptr<MockFetcher> fetcher = std::make_shared<MockFetcher>();
    etl::impl::Extractor extractor{fetcher};
};

TEST_F(ExtractorTests, ExtractLedgerWithDiffNoResult)
{
    EXPECT_CALL(*fetcher, fetchDataAndDiff(kSEQ)).WillOnce(testing::Return(std::nullopt));
    auto res = extractor.extractLedgerWithDiff(kSEQ);
    EXPECT_FALSE(res.has_value());
}

TEST_F(ExtractorTests, ExtractLedgerOnlyNoResult)
{
    EXPECT_CALL(*fetcher, fetchData(kSEQ)).WillOnce(testing::Return(std::nullopt));
    auto res = extractor.extractLedgerOnly(kSEQ);
    EXPECT_FALSE(res.has_value());
}

TEST_F(ExtractorTests, ExtractLedgerWithDiffWithResult)
{
    auto original = util::createDataAndDiff();

    EXPECT_CALL(*fetcher, fetchDataAndDiff(kSEQ)).WillOnce(testing::Return(original));
    auto res = extractor.extractLedgerWithDiff(kSEQ);

    EXPECT_TRUE(res.has_value());
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    EXPECT_EQ(res->objects.size(), 10);
    EXPECT_EQ(res->transactions.size(), 10);
    EXPECT_TRUE(res->successors.has_value());
    EXPECT_EQ(res->successors->size(), 10);
    EXPECT_FALSE(res->edgeKeys.has_value());  // this is set separately in ETL
    // NOLINTEND(bugprone-unchecked-optional-access)
}

TEST_F(ExtractorTests, ExtractLedgerOnlyWithResult)
{
    auto original = util::createData();

    EXPECT_CALL(*fetcher, fetchData(kSEQ)).WillOnce(testing::Return(original));
    auto res = extractor.extractLedgerOnly(kSEQ);

    EXPECT_TRUE(res.has_value());
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    EXPECT_TRUE(res->objects.empty());
    EXPECT_EQ(res->transactions.size(), 10);
    EXPECT_FALSE(res->successors.has_value());
    EXPECT_FALSE(res->edgeKeys.has_value());  // this is set separately in ETL
    // NOLINTEND(bugprone-unchecked-optional-access)
}
