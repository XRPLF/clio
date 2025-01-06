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

#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr auto kACCOUNT = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
constexpr auto kNFT_ID = "0008013AE1CD8B79A8BCB52335CD40DE97401B2D60A828720000099B00000000";
constexpr auto kNFT_ID2 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA";
constexpr auto kOFFER1 = "23F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";
constexpr auto kTX = "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";

}  // namespace

struct NFTHelpersTests : public NoLoggerFixture {};

TEST_F(NFTHelpersTests, ConvertDataFromNFTCancelOfferTx)
{
    auto const tx = createCancelNftOffersTxWithMetadata(kACCOUNT, 1, 2, std::vector<std::string>{kNFT_ID2, kNFT_ID});
    ripple::TxMeta const txMeta(ripple::uint256(kTX), 1, tx.metadata);
    auto const [nftTxs, nftDatas] =
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()}));

    EXPECT_EQ(nftTxs.size(), 2);
    EXPECT_FALSE(nftDatas);
}

TEST_F(NFTHelpersTests, ConvertDataFromNFTCancelOfferTxContainingDuplicateNFT)
{
    auto const tx = createCancelNftOffersTxWithMetadata(
        kACCOUNT, 1, 2, std::vector<std::string>{kNFT_ID2, kNFT_ID, kNFT_ID2, kNFT_ID}
    );
    ripple::TxMeta const txMeta(ripple::uint256(kTX), 1, tx.metadata);
    auto const [nftTxs, nftDatas] =
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()}));

    EXPECT_EQ(nftTxs.size(), 2);
    EXPECT_FALSE(nftDatas);
}

TEST_F(NFTHelpersTests, UniqueNFTDatas)
{
    std::vector<NFTsData> nftDatas;

    auto const generateNFTsData = [](char const* nftID, std::uint32_t txIndex) {
        auto const tx = createCreateNftOfferTxWithMetadata(kACCOUNT, 1, 50, nftID, 123, kOFFER1);
        ripple::SerialIter s{tx.metadata.data(), tx.metadata.size()};
        ripple::STObject meta{s, ripple::sfMetadata};
        meta.setFieldU32(ripple::sfTransactionIndex, txIndex);
        ripple::TxMeta const txMeta(ripple::uint256(kTX), 1, meta.getSerializer().peekData());

        auto const account = getAccountIdWithString(kACCOUNT);
        return NFTsData{ripple::uint256(nftID), account, ripple::Blob{}, txMeta};
    };

    nftDatas.push_back(generateNFTsData(kNFT_ID, 3));
    nftDatas.push_back(generateNFTsData(kNFT_ID, 1));
    nftDatas.push_back(generateNFTsData(kNFT_ID, 2));

    nftDatas.push_back(generateNFTsData(kNFT_ID2, 4));
    nftDatas.push_back(generateNFTsData(kNFT_ID2, 1));
    nftDatas.push_back(generateNFTsData(kNFT_ID2, 5));

    auto const uniqueNFTDatas = etl::getUniqueNFTsDatas(nftDatas);

    EXPECT_EQ(uniqueNFTDatas.size(), 2);
    EXPECT_EQ(uniqueNFTDatas[0].ledgerSequence, 1);
    EXPECT_EQ(uniqueNFTDatas[1].ledgerSequence, 1);
    EXPECT_EQ(uniqueNFTDatas[0].transactionIndex, 5);
    EXPECT_EQ(uniqueNFTDatas[1].transactionIndex, 3);
    EXPECT_EQ(uniqueNFTDatas[0].tokenID, ripple::uint256(kNFT_ID2));
    EXPECT_EQ(uniqueNFTDatas[1].tokenID, ripple::uint256(kNFT_ID));
}
