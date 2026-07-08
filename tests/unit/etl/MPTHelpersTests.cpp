#include "data/DBHelpers.hpp"
#include "etl/MPTHelpers.hpp"
#include "util/MPTokenTestObjects.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/MPTAmount.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STIssue.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
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

xrpl::STObject
createAccountRootNode(std::string_view account)
{
    xrpl::STObject fields(xrpl::sfFinalFields);
    fields.setAccountID(xrpl::sfAccount, getAccountIdWithString(account));

    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltACCOUNT_ROOT);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    node.set(std::move(fields));
    return node;
}

xrpl::STObject
createMPTokenNodeWithoutIssuanceID()
{
    xrpl::STObject fields(xrpl::sfFinalFields);
    fields.setAccountID(xrpl::sfAccount, getAccountIdWithString(kAccount));

    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    node.set(std::move(fields));
    return node;
}

xrpl::STObject
createMPTokenIssuanceNodeWithoutIssuer()
{
    xrpl::STObject fields(xrpl::sfFinalFields);
    fields.setFieldU32(xrpl::sfSequence, kIssuanceSeq);

    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN_ISSUANCE);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    node.set(std::move(fields));
    return node;
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
createTx(xrpl::TxType type)
{
    xrpl::STObject obj(xrpl::sfTransaction);
    obj.setFieldU16(xrpl::sfTransactionType, type);
    obj.setAccountID(xrpl::sfAccount, getAccountIdWithString(kAccount));
    obj.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    obj.setFieldU32(xrpl::sfSequence, 1);
    obj.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    // Satisfy the per-type required fields of the SOTemplate
    switch (type) {
        case xrpl::ttPAYMENT:
            obj.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(100, false));
            obj.setAccountID(xrpl::sfDestination, getAccountIdWithString(kAccount2));
            break;
        case xrpl::ttCLAWBACK:
            obj.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(100, false));
            break;
        case xrpl::ttOFFER_CREATE:
            obj.setFieldAmount(xrpl::sfTakerPays, xrpl::STAmount(100, false));
            obj.setFieldAmount(xrpl::sfTakerGets, xrpl::STAmount(200, false));
            break;
        case xrpl::ttAMM_DEPOSIT:
            obj.setFieldIssue(xrpl::sfAsset, xrpl::STIssue{xrpl::sfAsset, xrpl::xrpIssue()});
            obj.setFieldIssue(xrpl::sfAsset2, xrpl::STIssue{xrpl::sfAsset2, xrpl::xrpIssue()});
            break;
        case xrpl::ttMPTOKEN_ISSUANCE_DESTROY:
        case xrpl::ttMPTOKEN_ISSUANCE_SET:
            obj[xrpl::sfMPTokenIssuanceID] = defaultIssuanceID();
            break;
        default:
            break;
    }

    auto const serialized = obj.getSerializer();
    return xrpl::STTx{xrpl::SerialIter{serialized.slice()}};
}

xrpl::STTx
createPaymentTxWithMPTAmount(xrpl::uint192 const& issuanceID)
{
    xrpl::STObject obj(xrpl::sfTransaction);
    obj.setFieldU16(xrpl::sfTransactionType, xrpl::ttPAYMENT);
    obj.setAccountID(xrpl::sfAccount, getAccountIdWithString(kAccount));
    obj.setFieldAmount(
        xrpl::sfAmount, xrpl::STAmount(xrpl::MPTAmount{100}, xrpl::MPTIssue{issuanceID})
    );
    obj.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    obj.setAccountID(xrpl::sfDestination, getAccountIdWithString(kAccount2));
    obj.setFieldU32(xrpl::sfSequence, 1);
    obj.setFieldVL(xrpl::sfSigningPubKey, kSlice);

    auto const serialized = obj.getSerializer();
    return xrpl::STTx{xrpl::SerialIter{serialized.slice()}};
}

xrpl::STTx
createAMMDepositTxWithMPTIssue(xrpl::uint192 const& issuanceID)
{
    xrpl::STObject obj(xrpl::sfTransaction);
    obj.setFieldU16(xrpl::sfTransactionType, xrpl::ttAMM_DEPOSIT);
    obj.setAccountID(xrpl::sfAccount, getAccountIdWithString(kAccount));
    obj.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    obj.setFieldU32(xrpl::sfSequence, 1);
    obj.setFieldVL(xrpl::sfSigningPubKey, kSlice);
    obj.setFieldIssue(xrpl::sfAsset, xrpl::STIssue{xrpl::sfAsset, xrpl::MPTIssue{issuanceID}});
    obj.setFieldIssue(xrpl::sfAsset2, xrpl::STIssue{xrpl::sfAsset2, xrpl::xrpIssue()});

    auto const serialized = obj.getSerializer();
    return xrpl::STTx{xrpl::SerialIter{serialized.slice()}};
}

}  // namespace

struct MPTHelpersTest : virtual public ::testing::Test {
protected:
    static void
    verifyCommonFields(
        MPTokenIssuanceTransactionsData const& data,
        xrpl::STTx const& sttx,
        xrpl::TxMeta const& txMeta
    )
    {
        EXPECT_EQ(data.accounts, txMeta.getAffectedAccounts());
        EXPECT_EQ(data.ledgerSequence, txMeta.getLgrSeq());
        EXPECT_EQ(data.transactionIndex, txMeta.getIndex());
        EXPECT_EQ(data.txHash, sttx.getTransactionID());
    }
};

TEST_F(MPTHelpersTest, FailedTxWithoutIssuanceReferenceProducesNoRecords)
{
    std::vector<xrpl::STObject> nodes;
    auto const txMeta = createTxMeta(std::move(nodes), xrpl::tecINCOMPLETE);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, createTx(xrpl::ttPAYMENT));

    EXPECT_TRUE(records.empty());
}

TEST_F(MPTHelpersTest, FailedTxIgnoresMPTAffectedNodes)
{
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenIssuanceNode(xrpl::sfModifiedNode, kIssuanceSeq, kIssuer));
    nodes.push_back(util::createMPTokenNode(xrpl::sfCreatedNode, defaultIssuanceID(), kAccount));
    auto const txMeta = createTxMeta(std::move(nodes), xrpl::tecINCOMPLETE);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, createTx(xrpl::ttPAYMENT));

    EXPECT_TRUE(records.empty());
}

TEST_F(MPTHelpersTest, FailedTxWithTopLevelIssuanceIDProducesRecord)
{
    auto const txMeta = createTxMeta({}, xrpl::tecINCOMPLETE);
    auto const sttx = createTx(xrpl::ttMPTOKEN_ISSUANCE_SET);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, FailedTxWithMPTAmountProducesRecord)
{
    auto const txMeta = createTxMeta({}, xrpl::tecINCOMPLETE);
    auto const sttx = createPaymentTxWithMPTAmount(defaultIssuanceID());

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, FailedTxWithMPTIssueProducesRecord)
{
    auto const txMeta = createTxMeta({}, xrpl::tecINCOMPLETE);
    auto const sttx = createAMMDepositTxWithMPTIssue(defaultIssuanceID());

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, IssuanceCreateProducesRecordWithReconstructedID)
{
    auto const tx = createMPTIssuanceCreateTxWithMetadata(kIssuer, 2, kIssuanceSeq);
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, IssuanceDestroyProducesRecordFromDeletedNode)
{
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenIssuanceNode(xrpl::sfDeletedNode, kIssuanceSeq, kIssuer));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(xrpl::ttMPTOKEN_ISSUANCE_DESTROY);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, IssuanceSetProducesRecordFromModifiedNode)
{
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenIssuanceNode(xrpl::sfModifiedNode, kIssuanceSeq, kIssuer));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(xrpl::ttMPTOKEN_ISSUANCE_SET);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, AuthorizeProducesRecordFromMPTokenNode)
{
    auto const issuanceID = defaultIssuanceID();
    auto const tx = createMPTokenAuthorizeTxWithMetadata(kAccount, issuanceID, 2, 3);
    xrpl::TxMeta const txMeta(xrpl::uint256(kTX), 1, tx.metadata);
    auto const sttx = xrpl::STTx(xrpl::SerialIter{tx.transaction.data(), tx.transaction.size()});

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, issuanceID);
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, DedupsAcrossMPTokenAndIssuanceNodes)
{
    // Both nodes resolve to the same issuance ID: the MPToken node carries it directly while the
    // MPTokenIssuance node requires reconstruction via makeMptID.
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenNode(xrpl::sfCreatedNode, defaultIssuanceID(), kAccount));
    nodes.push_back(util::createMPTokenIssuanceNode(xrpl::sfModifiedNode, kIssuanceSeq, kIssuer));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(xrpl::ttPAYMENT);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    EXPECT_TRUE(records[0].accounts.contains(getAccountIdWithString(kAccount)));
    EXPECT_TRUE(records[0].accounts.contains(getAccountIdWithString(kIssuer)));
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, MultipleIssuancesFanOutAndDedup)
{
    auto const issuanceA = xrpl::makeMptID(1, getAccountIdWithString(kIssuer));
    auto const issuanceB = xrpl::makeMptID(2, getAccountIdWithString(kIssuer));
    ASSERT_LT(issuanceA, issuanceB);

    // issuanceA is touched twice and should produce only one index record.
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenNode(xrpl::sfCreatedNode, issuanceB, kAccount));
    nodes.push_back(util::createMPTokenNode(xrpl::sfModifiedNode, issuanceA, kAccount2));
    nodes.push_back(util::createMPTokenNode(xrpl::sfDeletedNode, issuanceA, kAccount));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(xrpl::ttPAYMENT);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 2);
    EXPECT_EQ(records[0].mptIssuanceID, issuanceA);
    EXPECT_EQ(records[1].mptIssuanceID, issuanceB);
    for (auto const& record : records) {
        EXPECT_EQ(record.accounts, txMeta.getAffectedAccounts());
        verifyCommonFields(record, sttx, txMeta);
    }
}

TEST_F(MPTHelpersTest, IndexesMPTNodesRegardlessOfTransactionType)
{
    constexpr xrpl::TxType kTypes[] = {
        xrpl::ttPAYMENT, xrpl::ttCLAWBACK, xrpl::ttOFFER_CREATE, xrpl::ttAMM_DEPOSIT
    };

    for (auto const type : kTypes) {
        std::vector<xrpl::STObject> nodes;
        nodes.push_back(
            util::createMPTokenNode(xrpl::sfModifiedNode, defaultIssuanceID(), kAccount)
        );
        auto const txMeta = createTxMeta(std::move(nodes));
        auto const sttx = createTx(type);

        auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

        ASSERT_EQ(records.size(), 1) << "TransactionType " << type;
        EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
        verifyCommonFields(records[0], sttx, txMeta);
    }
}

TEST_F(MPTHelpersTest, MPTNodeWithoutFieldsProducesNoRecords)
{
    // A ModifiedNode carrying no FinalFields must be skipped without crashing.
    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN_ISSUANCE);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});

    std::vector<xrpl::STObject> nodes;
    nodes.push_back(std::move(node));
    auto const txMeta = createTxMeta(std::move(nodes));

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, createTx(xrpl::ttPAYMENT));

    EXPECT_TRUE(records.empty());
}

TEST_F(MPTHelpersTest, MPTNodesMissingRequiredFieldsProduceNoRecords)
{
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(createMPTokenNodeWithoutIssuanceID());
    nodes.push_back(createMPTokenIssuanceNodeWithoutIssuer());
    auto const txMeta = createTxMeta(std::move(nodes));

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, createTx(xrpl::ttPAYMENT));

    EXPECT_TRUE(records.empty());
}

TEST_F(MPTHelpersTest, NoMPTNodesProducesNoRecords)
{
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(createAccountRootNode(kAccount));
    auto const txMeta = createTxMeta(std::move(nodes));

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, createTx(xrpl::ttPAYMENT));

    EXPECT_TRUE(records.empty());
}

TEST_F(MPTHelpersTest, RecordCarriesAllAffectedAccounts)
{
    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenNode(xrpl::sfCreatedNode, defaultIssuanceID(), kAccount));
    nodes.push_back(util::createMPTokenIssuanceNode(xrpl::sfModifiedNode, kIssuanceSeq, kIssuer));
    nodes.push_back(createAccountRootNode(kAccount2));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(xrpl::ttPAYMENT);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].accounts, txMeta.getAffectedAccounts());
    EXPECT_EQ(records[0].accounts.size(), 3);
    EXPECT_TRUE(records[0].accounts.contains(getAccountIdWithString(kAccount)));
    EXPECT_TRUE(records[0].accounts.contains(getAccountIdWithString(kAccount2)));
    EXPECT_TRUE(records[0].accounts.contains(getAccountIdWithString(kIssuer)));
}

TEST_F(MPTHelpersTest, ExtractionIsDeterministic)
{
    auto const issuanceA = xrpl::makeMptID(1, getAccountIdWithString(kIssuer));

    std::vector<xrpl::STObject> nodes;
    nodes.push_back(util::createMPTokenNode(xrpl::sfCreatedNode, issuanceA, kAccount));
    nodes.push_back(util::createMPTokenIssuanceNode(xrpl::sfModifiedNode, kIssuanceSeq, kIssuer));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(xrpl::ttPAYMENT);

    auto const first = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);
    auto const second = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(first.size(), 2);
    ASSERT_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        EXPECT_EQ(first[i].mptIssuanceID, second[i].mptIssuanceID);
        EXPECT_EQ(first[i].accounts, second[i].accounts);
        EXPECT_EQ(first[i].ledgerSequence, second[i].ledgerSequence);
        EXPECT_EQ(first[i].transactionIndex, second[i].transactionIndex);
        EXPECT_EQ(first[i].txHash, second[i].txHash);
    }
}
