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

#include "data/DBHelpers.hpp"
#include "etl/NFTHelpers.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <grpcpp/support/status.h>
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
#include <xrpl/protocol/tokens.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

constexpr static auto ACCOUNT = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
constexpr static auto ACCOUNT2 = "rnd1nHuzceyQDqnLH8urWNr4QBKt4v7WVk";
constexpr static auto NFTID = "0008013AE1CD8B79A8BCB52335CD40DE97401B2D60A828720000099B00000000";
constexpr static auto NFTID2 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA";
constexpr static auto OFFER1 = "23F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";
constexpr static auto TX = "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";
// Page index is a valid nft page for ACCOUNT
constexpr static auto PAGE_INDEX = "E1CD8B79A8BCB52335CD40DE97401B2D60A82872FFFFFFFFFFFFFFFFFFFFFFFF";
constexpr static auto OFFER_ID = "AA86CBF29770F72FA3FF4A5D9A9FA54D6F399A8E038F72393EF782224865E27F";

struct NFTHelpersTest : NoLoggerFixture {
protected:
    static void
    verifyNFTTransactionsData(
        NFTTransactionsData const& data,
        ripple::STTx const& sttx,
        ripple::TxMeta const& txMeta,
        std::string_view nftId
    )
    {
        EXPECT_EQ(data.tokenID, ripple::uint256(nftId));
        EXPECT_EQ(data.ledgerSequence, txMeta.getLgrSeq());
        EXPECT_EQ(data.transactionIndex, txMeta.getIndex());
        EXPECT_EQ(data.txHash, sttx.getTransactionID());
    }

    static void
    verifyNFTsData(
        NFTsData const& data,
        ripple::STTx const& sttx,
        ripple::TxMeta const& txMeta,
        std::string_view nftId,
        std::optional<std::string> const& owner
    )
    {
        EXPECT_EQ(data.tokenID, ripple::uint256(nftId));
        EXPECT_EQ(data.ledgerSequence, txMeta.getLgrSeq());
        EXPECT_EQ(data.transactionIndex, txMeta.getIndex());
        if (owner)
            EXPECT_EQ(data.owner, GetAccountIDWithString(*owner));

        if (sttx.getTxnType() == ripple::ttNFTOKEN_MINT || sttx.getTxnType() == ripple::ttNFTOKEN_MODIFY) {
            EXPECT_TRUE(data.uri.has_value());
            EXPECT_EQ(*data.uri, sttx.getFieldVL(ripple::sfURI));
        } else {
            EXPECT_FALSE(data.uri.has_value());
        }

        if (sttx.getTxnType() == ripple::ttNFTOKEN_BURN) {
            EXPECT_TRUE(data.isBurned);
        } else {
            EXPECT_FALSE(data.isBurned);
        }

        if (sttx.getTxnType() == ripple::ttNFTOKEN_MODIFY) {
            EXPECT_TRUE(data.onlyUriChanged);
        } else {
            EXPECT_FALSE(data.onlyUriChanged);
        }
    }
};

TEST_F(NFTHelpersTest, NFTDataFromFailedTx)
{
    auto const tx = CreateNFTModifyTxWithMetadata(ACCOUNT, NFTID, ripple::Blob{});

    // Inject a failed result
    ripple::SerialIter sitMeta(ripple::makeSlice(tx.metadata));
    ripple::STObject objMeta(sitMeta, ripple::sfMetadata);
    objMeta.setFieldU8(ripple::sfTransactionResult, ripple::tecINCOMPLETE);

    ripple::TxMeta txMeta(ripple::uint256(TX), 1, objMeta.getSerializer().peekData());
    auto const [nftTxs, nftDatas] =
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()}));

    EXPECT_EQ(nftTxs.size(), 0);
    EXPECT_FALSE(nftDatas);
}

TEST_F(NFTHelpersTest, NotNFTTx)
{
    auto const tx = CreateOracleSetTxWithMetadata(
        ACCOUNT,
        1,
        123,
        1,
        4321u,
        CreatePriceDataSeries({CreateOraclePriceData(1e3, ripple::to_currency("EUR"), ripple::to_currency("XRP"), 2)}),
        PAGE_INDEX,
        false,
        TX
    );

    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);

    auto const [nftTxs, nftDatas] =
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()}));

    EXPECT_EQ(nftTxs.size(), 0);
    EXPECT_FALSE(nftDatas);
}

TEST_F(NFTHelpersTest, NFTModifyWithURI)
{
    std::string const uri("1234567890A");
    ripple::Blob const uriBlob(uri.begin(), uri.end());

    auto const tx = CreateNFTModifyTxWithMetadata(ACCOUNT, NFTID, uriBlob);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);

    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] =
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()}));

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, std::nullopt);
}

TEST_F(NFTHelpersTest, NFTModifyWithoutURI)
{
    auto const tx = CreateNFTModifyTxWithMetadata(ACCOUNT, NFTID, ripple::Blob{});
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, std::nullopt);
}

TEST_F(NFTHelpersTest, NFTMintFromModifedNode)
{
    auto const tx = CreateMintNFTTxWithMetadata(ACCOUNT, 1, 20, 1, NFTID);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    txMeta.getNodes()[0].setFieldH256(ripple::sfLedgerIndex, ripple::uint256(PAGE_INDEX));
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, ACCOUNT);
}

TEST_F(NFTHelpersTest, NFTMintCantFindNewNFT)
{
    // No NFT added to the page
    auto const tx = CreateMintNFTTxWithMetadataOfCreatedNode(ACCOUNT, 1, 20, 1, std::nullopt, std::nullopt, PAGE_INDEX);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);

    EXPECT_THROW(
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()})),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTMintFromCreatedNode)
{
    std::string const uri("1234567890A");
    ripple::Blob const uriBlob(uri.begin(), uri.end());
    auto const tx = CreateMintNFTTxWithMetadataOfCreatedNode(ACCOUNT, 1, 20, 1, NFTID, uri, PAGE_INDEX);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});

    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, ACCOUNT);
}

TEST_F(NFTHelpersTest, NFTMintWithoutUriField)
{
    auto const tx = CreateMintNFTTxWithMetadataOfCreatedNode(ACCOUNT, 1, 20, 1, NFTID, std::nullopt, PAGE_INDEX);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});

    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, ACCOUNT);
}

TEST_F(NFTHelpersTest, NFTMintZeroMetaNode)
{
    auto const tx = CreateMintNFTTxWithMetadataOfCreatedNode(ACCOUNT, 1, 20, 1, NFTID, std::nullopt, PAGE_INDEX);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    txMeta.getNodes().clear();

    EXPECT_THROW(
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()})),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTBurnFromDeletedNode)
{
    auto const tx = CreateNFTBurnTxWithMetadataOfDeletedNode(ACCOUNT, NFTID);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    txMeta.getNodes()[1].setFieldH256(ripple::sfLedgerIndex, ripple::uint256(PAGE_INDEX));
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, ACCOUNT);
}

TEST_F(NFTHelpersTest, NFTBurnZeroMetaNode)
{
    auto const tx = CreateNFTBurnTxWithMetadataOfDeletedNode(ACCOUNT, NFTID);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    txMeta.getNodes().clear();

    EXPECT_THROW(
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()})),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTBurnFromModifiedNode)
{
    auto const tx = CreateNFTBurnTxWithMetadataOfModifiedNode(ACCOUNT, NFTID);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    txMeta.getNodes()[0].setFieldH256(ripple::sfLedgerIndex, ripple::uint256(PAGE_INDEX));

    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, ACCOUNT);
}

TEST_F(NFTHelpersTest, NFTCancelOffer)
{
    auto const tx = CreateCancelNFTOffersTxWithMetadata(ACCOUNT, 1, 2, std::vector<std::string>{NFTID, NFTID2});
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    txMeta.getNodes()[0].setFieldH256(ripple::sfLedgerIndex, ripple::uint256(PAGE_INDEX));
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 2);
    EXPECT_FALSE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTTransactionsData(nftTxs[1], sttx, txMeta, NFTID2);
}

TEST_F(NFTHelpersTest, NFTCancelOfferContainsDuplicateNFTs)
{
    auto const tx =
        CreateCancelNFTOffersTxWithMetadata(ACCOUNT, 1, 2, std::vector<std::string>{NFTID2, NFTID, NFTID2, NFTID});
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 2);
    EXPECT_FALSE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTTransactionsData(nftTxs[1], sttx, txMeta, NFTID2);
}

TEST_F(NFTHelpersTest, UniqueNFTDatas)
{
    std::vector<NFTsData> nftDatas;

    auto const generateNFTsData = [](char const* nftID, std::uint32_t txIndex) {
        auto const tx = CreateCreateNFTOfferTxWithMetadata(ACCOUNT, 1, 50, nftID, 123, OFFER1);
        ripple::SerialIter s{tx.metadata.data(), tx.metadata.size()};
        ripple::STObject meta{s, ripple::sfMetadata};
        meta.setFieldU32(ripple::sfTransactionIndex, txIndex);
        ripple::TxMeta const txMeta(ripple::uint256(TX), 1, meta.getSerializer().peekData());

        auto const account = GetAccountIDWithString(ACCOUNT);
        return NFTsData{ripple::uint256(nftID), account, ripple::Blob{}, txMeta};
    };

    nftDatas.push_back(generateNFTsData(NFTID, 3));
    nftDatas.push_back(generateNFTsData(NFTID, 1));
    nftDatas.push_back(generateNFTsData(NFTID, 2));

    nftDatas.push_back(generateNFTsData(NFTID2, 4));
    nftDatas.push_back(generateNFTsData(NFTID2, 1));
    nftDatas.push_back(generateNFTsData(NFTID2, 5));

    auto const uniqueNFTDatas = etl::getUniqueNFTsDatas(nftDatas);

    EXPECT_EQ(uniqueNFTDatas.size(), 2);
    EXPECT_EQ(uniqueNFTDatas[0].ledgerSequence, 1);
    EXPECT_EQ(uniqueNFTDatas[1].ledgerSequence, 1);
    EXPECT_EQ(uniqueNFTDatas[0].transactionIndex, 5);
    EXPECT_EQ(uniqueNFTDatas[1].transactionIndex, 3);
    EXPECT_EQ(uniqueNFTDatas[0].tokenID, ripple::uint256(NFTID2));
    EXPECT_EQ(uniqueNFTDatas[1].tokenID, ripple::uint256(NFTID));
}

TEST_F(NFTHelpersTest, NFTAcceptBuyerOffer)
{
    auto const tx = CreateAcceptNFTBuyerOfferTxWithMetadata(ACCOUNT, 1, 2, NFTID, OFFER_ID);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    EXPECT_TRUE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, ACCOUNT);
}

// The offer id in tx is different from the offer id in deleted node in metadata
TEST_F(NFTHelpersTest, NFTAcceptBuyerOfferCheckOfferIDFail)
{
    auto const tx = CreateAcceptNFTBuyerOfferTxWithMetadata(ACCOUNT, 1, 2, NFTID, OFFER_ID);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    // inject a different offer id
    txMeta.getNodes()[0].setFieldH256(ripple::sfLedgerIndex, ripple::uint256(PAGE_INDEX));

    EXPECT_THROW(
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()})),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferFromCreatedNode)
{
    auto const tx = CreateAcceptNFTSellerOfferTxWithMetadata(ACCOUNT2, 1, 2, NFTID, OFFER_ID, PAGE_INDEX, true);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    EXPECT_TRUE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, ACCOUNT);
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferFromModifiedNode)
{
    auto const tx = CreateAcceptNFTSellerOfferTxWithMetadata(ACCOUNT2, 1, 2, NFTID, OFFER_ID, PAGE_INDEX, false);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    EXPECT_TRUE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
    verifyNFTsData(*nftDatas, sttx, txMeta, NFTID, ACCOUNT);
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferCheckFail)
{
    // The only changed nft page is owned by ACCOUNT, thus can't find the new owner
    auto const tx = CreateAcceptNFTSellerOfferTxWithMetadata(ACCOUNT, 1, 2, NFTID, OFFER_ID, PAGE_INDEX, true);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);

    EXPECT_THROW(
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()})),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferNotInMeta)
{
    auto const tx = CreateAcceptNFTSellerOfferTxWithMetadata(ACCOUNT, 1, 2, NFTID, OFFER_ID, PAGE_INDEX, true);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    // inject a different offer id
    txMeta.getNodes()[0].setFieldH256(ripple::sfLedgerIndex, ripple::uint256(PAGE_INDEX));

    EXPECT_THROW(
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()})),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferZeroMetaNode)
{
    auto const tx = CreateAcceptNFTSellerOfferTxWithMetadata(ACCOUNT2, 1, 2, NFTID, OFFER_ID, PAGE_INDEX, true);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    txMeta.getNodes().clear();

    EXPECT_THROW(
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()})),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTAcceptSellerOfferIDNotInMetaData)
{
    auto const tx = CreateAcceptNFTSellerOfferTxWithMetadata(ACCOUNT2, 1, 2, NFTID, OFFER_ID, PAGE_INDEX, true);
    ripple::TxMeta txMeta(ripple::uint256(TX), 1, tx.metadata);
    // The first node is offer, the second is nft page. Change the offer id to something else
    txMeta.getNodes()[0]
        .getField(ripple::sfFinalFields)
        .downcast<ripple::STObject>()
        .setFieldH256(ripple::sfNFTokenID, ripple::uint256(NFTID2));

    EXPECT_THROW(
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()})),
        std::runtime_error
    );
}

TEST_F(NFTHelpersTest, NFTCreateOffer)
{
    auto const tx = CreateCreateNFTOfferTxWithMetadata(ACCOUNT, 1, 2, NFTID, 1, OFFER_ID);
    ripple::TxMeta txMeta(ripple::uint256(TX), 5, tx.metadata);
    auto const sttx = ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});
    auto const [nftTxs, nftDatas] = etl::getNFTDataFromTx(txMeta, sttx);

    EXPECT_EQ(nftTxs.size(), 1);
    EXPECT_FALSE(nftDatas);
    verifyNFTTransactionsData(nftTxs[0], sttx, txMeta, NFTID);
}

TEST_F(NFTHelpersTest, NFTDataFromLedgerObject)
{
    std::string const url1 = "abcd1";
    std::string const url2 = "abcd2";
    ripple::Blob const uri1Blob(url1.begin(), url1.end());
    ripple::Blob const uri2Blob(url2.begin(), url2.end());

    auto const nftPage = CreateNFTTokenPage({{NFTID, url1}, {NFTID2, url2}}, std::nullopt);
    auto const serializerNftPage = nftPage.getSerializer();

    int constexpr seq{5};
    auto const account = GetAccountIDWithString(ACCOUNT);

    auto const nftDatas = etl::getNFTDataFromObj(
        seq,
        std::string(static_cast<char const*>(static_cast<void const*>(account.data())), account.size()),
        std::string(static_cast<char const*>(serializerNftPage.getDataPtr()), serializerNftPage.getDataLength())
    );

    EXPECT_EQ(nftDatas.size(), 2);
    EXPECT_EQ(nftDatas[0].tokenID, ripple::uint256(NFTID));
    EXPECT_EQ(*(nftDatas[0].uri), uri1Blob);
    EXPECT_FALSE(nftDatas[0].onlyUriChanged);
    EXPECT_EQ(nftDatas[0].owner, account);
    EXPECT_EQ(nftDatas[0].ledgerSequence, seq);
    EXPECT_FALSE(nftDatas[0].isBurned);

    EXPECT_EQ(nftDatas[1].tokenID, ripple::uint256(NFTID2));
    EXPECT_EQ(*(nftDatas[1].uri), uri2Blob);
    EXPECT_FALSE(nftDatas[1].onlyUriChanged);
    EXPECT_EQ(nftDatas[1].owner, account);
    EXPECT_EQ(nftDatas[1].ledgerSequence, seq);
    EXPECT_FALSE(nftDatas[1].isBurned);
}
