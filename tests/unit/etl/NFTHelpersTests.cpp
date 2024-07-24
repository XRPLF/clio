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

#include "etl/NFTHelpers.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

#include <string>
#include <vector>

constexpr static auto ACCOUNT = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
constexpr static auto NFTID = "0008013AE1CD8B79A8BCB52335CD40DE97401B2D60A828720000099B00000000";
constexpr static auto NFTID2 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA";
constexpr static auto TX = "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";

struct NFTHelpersTests : public NoLoggerFixture {};

TEST_F(NFTHelpersTests, ConvertDataFromNFTCancelOfferTx)
{
    auto const tx = CreateCancelNFTOffersTxWithMetadata(ACCOUNT, 1, 2, std::vector<std::string>{NFTID2, NFTID});
    ripple::TxMeta const txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const [nftTxs, nftDatas] =
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()}));

    EXPECT_EQ(nftTxs.size(), 2);
    EXPECT_FALSE(nftDatas);
}

TEST_F(NFTHelpersTests, ConvertDataFromNFTCancelOfferTxContainingDuplicateNFT)
{
    auto const tx =
        CreateCancelNFTOffersTxWithMetadata(ACCOUNT, 1, 2, std::vector<std::string>{NFTID2, NFTID, NFTID2, NFTID});
    ripple::TxMeta const txMeta(ripple::uint256(TX), 1, tx.metadata);
    auto const [nftTxs, nftDatas] =
        etl::getNFTDataFromTx(txMeta, ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()}));

    EXPECT_EQ(nftTxs.size(), 2);
    EXPECT_FALSE(nftDatas);
}
