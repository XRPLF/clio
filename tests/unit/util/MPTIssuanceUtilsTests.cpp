#include "util/MPTIssuanceUtils.hpp"
#include "util/MPTokenTestObjects.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace {

constexpr auto kAccount = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
constexpr auto kAccount2 = "rnd1nHuzceyQDqnLH8urWNr4QBKt4v7WVk";
constexpr auto kIssuer = "rK1EX542EgA9m948JrJRaEzwLVEhqWvnr9";
constexpr auto kTX = "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";
constexpr std::uint32_t kIssuanceSeq = 7;
constexpr std::uint32_t kLedgerSeq = 99;
constexpr std::uint32_t kTxIndex = 4;

xrpl::Slice const kSlice("test", 4);

xrpl::uint192
defaultIssuanceID()
{
    return xrpl::makeMptID(kIssuanceSeq, getAccountIdWithString(kIssuer));
}

xrpl::TxMeta
createTxMeta(std::vector<xrpl::STObject> nodes, int result = xrpl::tesSUCCESS)
{
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldU8(xrpl::sfTransactionResult, result);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, kTxIndex);

    xrpl::STArray affectedNodes(xrpl::sfAffectedNodes);
    for (auto& node : nodes)
        affectedNodes.push_back(std::move(node));
    metaObj.setFieldArray(xrpl::sfAffectedNodes, affectedNodes);

    return xrpl::TxMeta{xrpl::uint256(kTX), kLedgerSeq, metaObj.getSerializer().peekData()};
}

xrpl::STTx
createPaymentTx()
{
    xrpl::STObject obj(xrpl::sfTransaction);
    obj.setFieldU16(xrpl::sfTransactionType, xrpl::ttPAYMENT);
    obj.setAccountID(xrpl::sfAccount, getAccountIdWithString(kAccount));
    obj.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(100, false));
    obj.setAccountID(xrpl::sfDestination, getAccountIdWithString(kAccount2));
    obj.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    obj.setFieldU32(xrpl::sfSequence, 1);
    obj.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    auto const serialized = obj.getSerializer();
    return xrpl::STTx{xrpl::SerialIter{serialized.slice()}};
}

xrpl::STTx
createMptIssuanceSetTx()
{
    xrpl::STObject obj(xrpl::sfTransaction);
    obj.setFieldU16(xrpl::sfTransactionType, xrpl::ttMPTOKEN_ISSUANCE_SET);
    obj.setAccountID(xrpl::sfAccount, getAccountIdWithString(kAccount));
    obj.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    obj.setFieldU32(xrpl::sfSequence, 1);
    obj.setFieldVL(xrpl::sfSigningPubKey, kSlice);
    obj[xrpl::sfMPTokenIssuanceID] = defaultIssuanceID();

    auto const serialized = obj.getSerializer();
    return xrpl::STTx{xrpl::SerialIter{serialized.slice()}};
}

}  // namespace

TEST(MPTIssuanceUtilsTest, ReferencesMptIssuance_MatchesFromAffectedNode)
{
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenNode(xrpl::sfCreatedNode, defaultIssuanceID(), kAccount));
    auto const txMeta = createTxMeta(std::move(nodes));

    EXPECT_TRUE(util::referencesMptIssuance(txMeta, createPaymentTx(), defaultIssuanceID()));
}

TEST(MPTIssuanceUtilsTest, ReferencesMptIssuance_NoMatchWhenIssuanceDiffers)
{
    auto const otherIssuance = xrpl::makeMptID(kIssuanceSeq + 1, getAccountIdWithString(kIssuer));
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenNode(xrpl::sfCreatedNode, otherIssuance, kAccount));
    auto const txMeta = createTxMeta(std::move(nodes));

    EXPECT_FALSE(util::referencesMptIssuance(txMeta, createPaymentTx(), defaultIssuanceID()));
}

TEST(MPTIssuanceUtilsTest, ReferencesMptIssuance_FailedTxIgnoresAffectedNodes)
{
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenNode(xrpl::sfCreatedNode, defaultIssuanceID(), kAccount));
    auto const txMeta = createTxMeta(std::move(nodes), xrpl::tecINCOMPLETE);

    EXPECT_FALSE(util::referencesMptIssuance(txMeta, createPaymentTx(), defaultIssuanceID()));
}

TEST(MPTIssuanceUtilsTest, ReferencesMptIssuance_FailedTxStillMatchesViaOwnFields)
{
    // A failed transaction has no meaningful affected nodes, but its own fields (e.g.
    // sfMPTokenIssuanceID) are scanned regardless of the transaction result.
    auto const txMeta = createTxMeta({}, xrpl::tecINCOMPLETE);

    EXPECT_TRUE(util::referencesMptIssuance(txMeta, createMptIssuanceSetTx(), defaultIssuanceID()));
}

TEST(MPTIssuanceUtilsTest, ReferencesMptIssuance_NoReferenceReturnsFalse)
{
    auto const txMeta = createTxMeta({});

    EXPECT_FALSE(util::referencesMptIssuance(txMeta, createPaymentTx(), defaultIssuanceID()));
}
