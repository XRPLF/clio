#include "data/DBHelpers.hpp"
#include "etl/NFTHelpers.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr auto kAccount = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
constexpr auto kAccount2 = "rnd1nHuzceyQDqnLH8urWNr4QBKt4v7WVk";
constexpr auto kNftId = "0008013AE1CD8B79A8BCB52335CD40DE97401B2D60A828720000099B00000000";
constexpr auto kNftID2 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA";
constexpr auto kOffer1 = "23F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";
constexpr auto kTX = "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";
// Page index is a valid nft page for ACCOUNT
constexpr auto kPageIndex = "E1CD8B79A8BCB52335CD40DE97401B2D60A82872FFFFFFFFFFFFFFFFFFFFFFFF";
constexpr auto kOfferId = "AA86CBF29770F72FA3FF4A5D9A9FA54D6F399A8E038F72393EF782224865E27F";

}  // namespace

struct NFTHelpersTest : virtual public ::testing::Test {
protected:
    static void
    verifyNFTTransactionsData(
        NFTTransactionsData const& data,
        xrpl::STTx const& sttx,
        xrpl::TxMeta const& txMeta,
        std::string_view nftId
    )
    {
        EXPECT_EQ(data.tokenID, xrpl::uint256(nftId));
        EXPECT_EQ(data.ledgerSequence, txMeta.getLgrSeq());
        EXPECT_EQ(data.transactionIndex, txMeta.getIndex());
        EXPECT_EQ(data.txHash, sttx.getTransactionID());
    }

    static void
    verifyNFTsData(
        NFTsData const& data,
        xrpl::STTx const& sttx,
        xrpl::TxMeta const& txMeta,
        std::string_view nftId,
        std::optional<std::string> const& owner
    )
    {
        EXPECT_EQ(data.tokenID, xrpl::uint256(nftId));
        EXPECT_EQ(data.ledgerSequence, txMeta.getLgrSeq());
        EXPECT_EQ(data.transactionIndex, txMeta.getIndex());
        if (owner)
            EXPECT_EQ(data.owner, getAccountIdWithString(*owner));

        if (sttx.getTxnType() == xrpl::ttNFTOKEN_MINT ||
            sttx.getTxnType() == xrpl::ttNFTOKEN_MODIFY) {
            ASSERT_TRUE(data.uri.has_value());
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            EXPECT_EQ(*data.uri, sttx.getFieldVL(xrpl::sfURI));
        } else {
            EXPECT_FALSE(data.uri.has_value());
        }

        if (sttx.getTxnType() == xrpl::ttNFTOKEN_BURN) {
            EXPECT_TRUE(data.isBurned);
        } else {
            EXPECT_FALSE(data.isBurned);
        }

        if (sttx.getTxnType() == xrpl::ttNFTOKEN_MODIFY) {
            EXPECT_TRUE(data.onlyUriChanged);
        } else {
            EXPECT_FALSE(data.onlyUriChanged);
        }
    }
};

TEST_F(NFTHelpersTest, NFTDataFromFailedTx)
{
    auto const tx = createNftModifyTxWithMetadata(kAccount, kNftId, xrpl::Blob{});

    // Inject a failed result
    xrpl::SerialIter sitMeta(xrpl::makeSlice(tx.metadata));
    xrpl::STObject objMeta(sitMeta, xrpl::sfMetadata);
    objMeta.setFieldU8(xrpl::sfTransactionResult, xrpl::tecINCOMPLETE);

    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, objMeta.getSerializer().peekData());
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(
        txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
    );

    EXPECT_EQ(nftTxs.size(), 0);
    EXPECT_FALSE(nftDatas);
}

TEST_F(NFTHelpersTest, NotNFTTx)
{
    auto const tx = createOracleSetTxWithMetadata(
        kAccount,
        1,
        123,
        1,
        4321u,
        createPriceDataSeries(
            {createOraclePriceData(1e3, xrpl::toCurrency("EUR"), xrpl::toCurrency("XRP"), 2)}
        ),
        kPageIndex,
        false,
        kTX
    );

    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);

    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(
        txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
    );

    EXPECT_EQ(nftTxs.size(), 0);
    EXPECT_FALSE(nftDatas);
}

TEST_F(NFTHelpersTest, NFTModifyWithURI)
{
    std::string const uri("1234567890A");
    xrpl::Blob const uriBlob(uri.begin(), uri.end());

    auto const tx = createNftModifyTxWithMetadata(kAccount, kNftId, uriBlob);
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);

    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(
        txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
    );

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, std::nullopt);
}

TEST_F(NFTHelpersTest, NFTModifyWithoutURI)
{
    auto const tx = createNftModifyTxWithMetadata(kAccount, kNftId, xrpl::Blob{});
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, std::nullopt);
}

TEST_F(NFTHelpersTest, NFTMintFromModifiedNode)
{
    auto const tx = createMintNftTxWithMetadata(kAccount, 1, 20, 1, kNftId);
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    txMeta.getNodes()[0].setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256(kPageIndex));
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, kAccount);
}

TEST_F(NFTHelpersTest, NFTMintCantFindNewNFT)
{
    // No NFT added to the page
    auto const tx = createMintNftTxWithMetadataOfCreatedNode(
        kAccount, 1, 20, 1, std::nullopt, std::nullopt, kPageIndex
    );
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);

    EXPECT_THROW(
        etl::getNFTDataFromTx(
            txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
        ),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTMintFromCreatedNode)
{
    std::string const uri("1234567890A");
    xrpl::Blob const uriBlob(uri.begin(), uri.end());
    auto const tx =
        createMintNftTxWithMetadataOfCreatedNode(kAccount, 1, 20, 1, kNftId, uri, kPageIndex);
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});

    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, kAccount);
}

TEST_F(NFTHelpersTest, NFTMintWithoutUriField)
{
    auto const tx = createMintNftTxWithMetadataOfCreatedNode(
        kAccount, 1, 20, 1, kNftId, std::nullopt, kPageIndex
    );
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});

    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, kAccount);
}

TEST_F(NFTHelpersTest, NFTMintZeroMetaNode)
{
    auto const tx = createMintNftTxWithMetadataOfCreatedNode(
        kAccount, 1, 20, 1, kNftId, std::nullopt, kPageIndex
    );
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    txMeta.getNodes().clear();

    EXPECT_THROW(
        etl::getNFTDataFromTx(
            txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
        ),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTBurnFromDeletedNode)
{
    auto const tx = createNftBurnTxWithMetadataOfDeletedNode(kAccount, kNftId);
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    txMeta.getNodes()[1].setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256(kPageIndex));
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, kAccount);
}

TEST_F(NFTHelpersTest, NFTBurnZeroMetaNode)
{
    auto const tx = createNftBurnTxWithMetadataOfDeletedNode(kAccount, kNftId);
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    txMeta.getNodes().clear();

    EXPECT_THROW(
        etl::getNFTDataFromTx(
            txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
        ),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTBurnFromModifiedNode)
{
    auto const tx = createNftBurnTxWithMetadataOfModifiedNode(kAccount, kNftId);
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    txMeta.getNodes()[0].setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256(kPageIndex));

    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, kAccount);
}

TEST_F(NFTHelpersTest, NFTCancelOffer)
{
    auto const tx = createCancelNftOffersTxWithMetadata(
        kAccount, 1, 2, std::vector<std::string>{kNftId, kNftID2}
    );
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    txMeta.getNodes()[0].setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256(kPageIndex));
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 2);
    EXPECT_FALSE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    verifyNFTTransactionsData(nftTxs[1], sttx, txMeta, kNftID2);
}

TEST_F(NFTHelpersTest, NFTCancelOfferContainsDuplicateNFTs)
{
    auto const tx = createCancelNftOffersTxWithMetadata(
        kAccount, 1, 2, std::vector<std::string>{kNftID2, kNftId, kNftID2, kNftId}
    );
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 2);
    EXPECT_FALSE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    verifyNFTTransactionsData(nftTxs[1], sttx, txMeta, kNftID2);
}

TEST_F(NFTHelpersTest, UniqueNFTDatas)
{
    std::vector<NFTsData> nftDatas;

    auto const generateNFTsData = [](char const* nftID, std::uint32_t txIndex) {
        auto const tx = createCreateNftOfferTxWithMetadata(kAccount, 1, 50, nftID, 123, kOffer1);
        xrpl::SerialIter s{tx.metadata.data(), tx.metadata.size()};
        xrpl::STObject meta{s, xrpl::sfMetadata};
        meta.setFieldU32(xrpl::sfTransactionIndex, txIndex);
        xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, meta.getSerializer().peekData());

        auto const account = getAccountIdWithString(kAccount);
        return NFTsData{xrpl::uint256(nftID), account, xrpl::Blob{}, txMeta};
    };

    nftDatas.push_back(generateNFTsData(kNftId, 3));
    nftDatas.push_back(generateNFTsData(kNftId, 1));
    nftDatas.push_back(generateNFTsData(kNftId, 2));

    nftDatas.push_back(generateNFTsData(kNftID2, 4));
    nftDatas.push_back(generateNFTsData(kNftID2, 1));
    nftDatas.push_back(generateNFTsData(kNftID2, 5));

    auto const uniqueNFTDatas = etl::getUniqueNFTsDatas(nftDatas);

    EXPECT_EQ(uniqueNFTDatas.size(), 2);
    EXPECT_EQ(uniqueNFTDatas[0].ledgerSequence, 1);
    EXPECT_EQ(uniqueNFTDatas[1].ledgerSequence, 1);
    EXPECT_EQ(uniqueNFTDatas[0].transactionIndex, 5);
    EXPECT_EQ(uniqueNFTDatas[1].transactionIndex, 3);
    EXPECT_EQ(uniqueNFTDatas[0].tokenID, xrpl::uint256(kNftID2));
    EXPECT_EQ(uniqueNFTDatas[1].tokenID, xrpl::uint256(kNftId));
}

TEST_F(NFTHelpersTest, NFTAcceptBuyerOffer)
{
    auto const tx = createAcceptNftBuyerOfferTxWithMetadata(kAccount, 1, 2, kNftId, kOfferId);
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    EXPECT_TRUE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, kAccount);
}

// The offer id in tx is different from the offer id in deleted node in metadata
TEST_F(NFTHelpersTest, NFTAcceptBuyerOfferCheckOfferIDFail)
{
    auto const tx = createAcceptNftBuyerOfferTxWithMetadata(kAccount, 1, 2, kNftId, kOfferId);
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    // inject a different offer id
    txMeta.getNodes()[0].setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256(kPageIndex));

    EXPECT_THROW(
        etl::getNFTDataFromTx(
            txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
        ),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferFromCreatedNode)
{
    auto const tx = createAcceptNftSellerOfferTxWithMetadata(
        kAccount2, 1, 2, kNftId, kOfferId, kPageIndex, true
    );
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    EXPECT_TRUE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, kAccount);
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferFromModifiedNode)
{
    auto const tx = createAcceptNftSellerOfferTxWithMetadata(
        kAccount2, 1, 2, kNftId, kOfferId, kPageIndex, false
    );
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    EXPECT_TRUE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
    ASSERT_TRUE(nftDatas.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    verifyNFTsData(*nftDatas, sttx, txMeta, kNftId, kAccount);
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferCheckFail)
{
    // The only changed nft page is owned by ACCOUNT, thus can't find the new owner
    auto const tx = createAcceptNftSellerOfferTxWithMetadata(
        kAccount, 1, 2, kNftId, kOfferId, kPageIndex, true
    );
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);

    EXPECT_THROW(
        etl::getNFTDataFromTx(
            txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
        ),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferNotInMeta)
{
    auto const tx = createAcceptNftSellerOfferTxWithMetadata(
        kAccount, 1, 2, kNftId, kOfferId, kPageIndex, true
    );
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    // inject a different offer id
    txMeta.getNodes()[0].setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256(kPageIndex));

    EXPECT_THROW(
        etl::getNFTDataFromTx(
            txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
        ),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferZeroMetaNode)
{
    auto const tx = createAcceptNftSellerOfferTxWithMetadata(
        kAccount2, 1, 2, kNftId, kOfferId, kPageIndex, true
    );
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    txMeta.getNodes().clear();

    EXPECT_THROW(
        etl::getNFTDataFromTx(
            txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
        ),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferIDNotInMetaData)
{
    auto const tx = createAcceptNftSellerOfferTxWithMetadata(
        kAccount2, 1, 2, kNftId, kOfferId, kPageIndex, true
    );
    xrpl::TxMeta txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    // The first node is offer, the second is nft page. Change the offer id to something else
    txMeta.getNodes()[0]
        .getField(xrpl::sfFinalFields)
        .downcast<xrpl::STObject>()
        .setFieldH256(xrpl::sfNFTokenID, xrpl::uint256(kNftID2));

    EXPECT_THROW(
        etl::getNFTDataFromTx(
            txMeta, xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()})
        ),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTCreateOffer)
{
    auto const tx = createCreateNftOfferTxWithMetadata(kAccount, 1, 2, kNftId, 1, kOfferId);
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 5, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    EXPECT_FALSE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, kNftId);
}

TEST_F(NFTHelpersTest, NFTDataFromLedgerObject)
{
    std::string const url1 = "abcd1";
    std::string const url2 = "abcd2";
    xrpl::Blob const uri1Blob(url1.begin(), url1.end());
    xrpl::Blob const uri2Blob(url2.begin(), url2.end());

    auto const account = getAccountIdWithString(kAccount);
    auto const nftPage = createNftTokenPage({{kNftId, url1}, {kNftID2, url2}}, std::nullopt);
    auto const serializerNftPage = nftPage.getSerializer();
    auto const blob = std::string(
        static_cast<char const*>(serializerNftPage.getDataPtr()), serializerNftPage.getDataLength()
    );

    // key is a token made up from owner's account ID followed by unused (in Clio) value described
    // here:
    // https://github.com/XRPLF/XRPL-Standards/tree/master/XLS-0020-non-fungible-tokens#tokenpage-id-format
    constexpr auto kExtraBytes = "000000000000";
    auto const key = std::string(std::begin(account), std::end(account)) + kExtraBytes;

    uint32_t constexpr kSeq{5};
    auto const nftDatas = etl::getNFTDataFromObj(kSeq, key, blob);

    EXPECT_EQ(nftDatas.size(), 2);
    EXPECT_EQ(nftDatas[0].tokenID, xrpl::uint256(kNftId));
    EXPECT_EQ(*(nftDatas[0].uri), uri1Blob);  // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_FALSE(nftDatas[0].onlyUriChanged);
    EXPECT_EQ(nftDatas[0].owner, account);
    EXPECT_EQ(nftDatas[0].ledgerSequence, kSeq);
    EXPECT_FALSE(nftDatas[0].isBurned);

    EXPECT_EQ(nftDatas[1].tokenID, xrpl::uint256(kNftID2));
    EXPECT_EQ(*(nftDatas[1].uri), uri2Blob);  // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_FALSE(nftDatas[1].onlyUriChanged);
    EXPECT_EQ(nftDatas[1].owner, account);
    EXPECT_EQ(nftDatas[1].ledgerSequence, kSeq);
    EXPECT_FALSE(nftDatas[1].isBurned);
}
