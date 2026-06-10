#include "data/DBHelpers.hpp"
#include "etl/MPTHelpers.hpp"
#include "util/TestObject.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
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

ripple::Slice const kSlice("test", 4);

ripple::uint192
defaultIssuanceID()
{
    return ripple::makeMptID(kIssuanceSeq, getAccountIdWithString(kIssuer));
}

ripple::STObject
createMPTokenNode(
    ripple::SField const& nodeType,
    ripple::uint192 const& issuanceID,
    std::string_view holder
)
{
    auto const& fieldsName =
        nodeType == ripple::sfCreatedNode ? ripple::sfNewFields : ripple::sfFinalFields;

    ripple::STObject fields(fieldsName);
    fields.setAccountID(ripple::sfAccount, getAccountIdWithString(holder));
    fields[ripple::sfMPTokenIssuanceID] = issuanceID;

    ripple::STObject node(nodeType);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN);
    node.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{});
    node.emplace_back(std::move(fields));
    return node;
}

ripple::STObject
createMPTokenIssuanceNode(
    ripple::SField const& nodeType,
    std::uint32_t seq,
    std::string_view issuer
)
{
    auto const& fieldsName =
        nodeType == ripple::sfCreatedNode ? ripple::sfNewFields : ripple::sfFinalFields;

    ripple::STObject fields(fieldsName);
    fields.setFieldU32(ripple::sfSequence, seq);
    fields.setAccountID(ripple::sfIssuer, getAccountIdWithString(issuer));

    ripple::STObject node(nodeType);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN_ISSUANCE);
    node.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{});
    node.emplace_back(std::move(fields));
    return node;
}

ripple::STObject
createAccountRootNode(std::string_view account)
{
    ripple::STObject fields(ripple::sfFinalFields);
    fields.setAccountID(ripple::sfAccount, getAccountIdWithString(account));

    ripple::STObject node(ripple::sfModifiedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltACCOUNT_ROOT);
    node.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{});
    node.emplace_back(std::move(fields));
    return node;
}

ripple::TxMeta
createTxMeta(std::vector<ripple::STObject> nodes, int result = ripple::tesSUCCESS)
{
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldU8(ripple::sfTransactionResult, result);
    metaObj.setFieldU32(ripple::sfTransactionIndex, kTxIndex);

    ripple::STArray affectedNodes(ripple::sfAffectedNodes);
    for (auto& node : nodes)
        affectedNodes.push_back(std::move(node));
    metaObj.setFieldArray(ripple::sfAffectedNodes, affectedNodes);

    return ripple::TxMeta{ripple::uint256(kTX), kLedgerSeq, metaObj.getSerializer().peekData()};
}

ripple::STTx
createTx(ripple::TxType type)
{
    ripple::STObject obj(ripple::sfTransaction);
    obj.setFieldU16(ripple::sfTransactionType, type);
    obj.setAccountID(ripple::sfAccount, getAccountIdWithString(kAccount));
    obj.setFieldAmount(ripple::sfFee, ripple::STAmount(10, false));
    obj.setFieldU32(ripple::sfSequence, 1);
    obj.setFieldVL(ripple::sfSigningPubKey, kSlice);

    // Satisfy the per-type required fields of the SOTemplate
    switch (type) {
        case ripple::ttPAYMENT:
            obj.setFieldAmount(ripple::sfAmount, ripple::STAmount(100, false));
            obj.setAccountID(ripple::sfDestination, getAccountIdWithString(kAccount2));
            break;
        case ripple::ttCLAWBACK:
            obj.setFieldAmount(ripple::sfAmount, ripple::STAmount(100, false));
            break;
        case ripple::ttOFFER_CREATE:
            obj.setFieldAmount(ripple::sfTakerPays, ripple::STAmount(100, false));
            obj.setFieldAmount(ripple::sfTakerGets, ripple::STAmount(200, false));
            break;
        case ripple::ttAMM_DEPOSIT:
            obj.setFieldIssue(
                ripple::sfAsset, ripple::STIssue{ripple::sfAsset, ripple::xrpIssue()}
            );
            obj.setFieldIssue(
                ripple::sfAsset2, ripple::STIssue{ripple::sfAsset2, ripple::xrpIssue()}
            );
            break;
        case ripple::ttMPTOKEN_ISSUANCE_DESTROY:
        case ripple::ttMPTOKEN_ISSUANCE_SET:
            obj[ripple::sfMPTokenIssuanceID] = defaultIssuanceID();
            break;
        default:
            break;
    }

    auto const serialized = obj.getSerializer();
    return ripple::STTx{ripple::SerialIter{serialized.slice()}};
}

}  // namespace

struct MPTHelpersTest : virtual public ::testing::Test {
protected:
    static void
    verifyCommonFields(
        MPTokenIssuanceTransactionsData const& data,
        ripple::STTx const& sttx,
        ripple::TxMeta const& txMeta
    )
    {
        EXPECT_EQ(data.accounts, txMeta.getAffectedAccounts());
        EXPECT_EQ(data.ledgerSequence, txMeta.getLgrSeq());
        EXPECT_EQ(data.transactionIndex, txMeta.getIndex());
        EXPECT_EQ(data.txHash, sttx.getTransactionID());
    }
};

TEST_F(MPTHelpersTest, FailedTxProducesNoRecords)
{
    std::vector<ripple::STObject> nodes;
    nodes.push_back(createMPTokenNode(ripple::sfCreatedNode, defaultIssuanceID(), kAccount));
    auto const txMeta = createTxMeta(std::move(nodes), ripple::tecINCOMPLETE);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, createTx(ripple::ttPAYMENT));

    EXPECT_TRUE(records.empty());
}

TEST_F(MPTHelpersTest, IssuanceCreateProducesRecordWithReconstructedID)
{
    auto const tx = createMPTIssuanceCreateTxWithMetadata(kIssuer, 2, kIssuanceSeq);
    ripple::TxMeta const txMeta(ripple::uint256(kTX), 1, tx.metadata);
    auto const sttx =
        ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, IssuanceDestroyProducesRecordFromDeletedNode)
{
    std::vector<ripple::STObject> nodes;
    nodes.push_back(createMPTokenIssuanceNode(ripple::sfDeletedNode, kIssuanceSeq, kIssuer));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(ripple::ttMPTOKEN_ISSUANCE_DESTROY);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, IssuanceSetProducesRecordFromModifiedNode)
{
    std::vector<ripple::STObject> nodes;
    nodes.push_back(createMPTokenIssuanceNode(ripple::sfModifiedNode, kIssuanceSeq, kIssuer));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(ripple::ttMPTOKEN_ISSUANCE_SET);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, AuthorizeProducesRecordFromMPTokenNode)
{
    auto const issuanceID = defaultIssuanceID();
    auto const tx = createMPTokenAuthorizeTxWithMetadata(kAccount, issuanceID, 2, 3);
    ripple::TxMeta const txMeta(ripple::uint256(kTX), 1, tx.metadata);
    auto const sttx =
        ripple::STTx(ripple::SerialIter{tx.transaction.data(), tx.transaction.size()});

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, issuanceID);
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, DedupsAcrossMPTokenAndIssuanceNodes)
{
    // Both nodes resolve to the same issuance ID: the MPToken node carries it verbatim while the
    // MPTokenIssuance node requires reconstruction via makeMptID
    std::vector<ripple::STObject> nodes;
    nodes.push_back(createMPTokenNode(ripple::sfCreatedNode, defaultIssuanceID(), kAccount));
    nodes.push_back(createMPTokenIssuanceNode(ripple::sfModifiedNode, kIssuanceSeq, kIssuer));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(ripple::ttPAYMENT);

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);

    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].mptIssuanceID, defaultIssuanceID());
    EXPECT_TRUE(records[0].accounts.contains(getAccountIdWithString(kAccount)));
    EXPECT_TRUE(records[0].accounts.contains(getAccountIdWithString(kIssuer)));
    verifyCommonFields(records[0], sttx, txMeta);
}

TEST_F(MPTHelpersTest, MultipleIssuancesFanOutAndDedup)
{
    auto const issuanceA = ripple::makeMptID(1, getAccountIdWithString(kIssuer));
    auto const issuanceB = ripple::makeMptID(2, getAccountIdWithString(kIssuer));
    ASSERT_LT(issuanceA, issuanceB);

    // Two distinct issuances; issuanceA is touched twice and must be deduped
    std::vector<ripple::STObject> nodes;
    nodes.push_back(createMPTokenNode(ripple::sfCreatedNode, issuanceB, kAccount));
    nodes.push_back(createMPTokenNode(ripple::sfModifiedNode, issuanceA, kAccount2));
    nodes.push_back(createMPTokenNode(ripple::sfDeletedNode, issuanceA, kAccount));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(ripple::ttPAYMENT);

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
    constexpr ripple::TxType kTypes[] = {
        ripple::ttPAYMENT, ripple::ttCLAWBACK, ripple::ttOFFER_CREATE, ripple::ttAMM_DEPOSIT
    };

    for (auto const type : kTypes) {
        std::vector<ripple::STObject> nodes;
        nodes.push_back(createMPTokenNode(ripple::sfModifiedNode, defaultIssuanceID(), kAccount));
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
    // A ModifiedNode carrying no FinalFields must be skipped, not crash
    ripple::STObject node(ripple::sfModifiedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN_ISSUANCE);
    node.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{});

    std::vector<ripple::STObject> nodes;
    nodes.push_back(std::move(node));
    auto const txMeta = createTxMeta(std::move(nodes));

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, createTx(ripple::ttPAYMENT));

    EXPECT_TRUE(records.empty());
}

TEST_F(MPTHelpersTest, NoMPTNodesProducesNoRecords)
{
    std::vector<ripple::STObject> nodes;
    nodes.push_back(createAccountRootNode(kAccount));
    auto const txMeta = createTxMeta(std::move(nodes));

    auto const records = etl::getMPTokenIssuanceTxsFromTx(txMeta, createTx(ripple::ttPAYMENT));

    EXPECT_TRUE(records.empty());
}

TEST_F(MPTHelpersTest, RecordCarriesAllAffectedAccounts)
{
    std::vector<ripple::STObject> nodes;
    nodes.push_back(createMPTokenNode(ripple::sfCreatedNode, defaultIssuanceID(), kAccount));
    nodes.push_back(createMPTokenIssuanceNode(ripple::sfModifiedNode, kIssuanceSeq, kIssuer));
    nodes.push_back(createAccountRootNode(kAccount2));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(ripple::ttPAYMENT);

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
    auto const issuanceA = ripple::makeMptID(1, getAccountIdWithString(kIssuer));

    std::vector<ripple::STObject> nodes;
    nodes.push_back(createMPTokenNode(ripple::sfCreatedNode, issuanceA, kAccount));
    nodes.push_back(createMPTokenIssuanceNode(ripple::sfModifiedNode, kIssuanceSeq, kIssuer));
    auto const txMeta = createTxMeta(std::move(nodes));
    auto const sttx = createTx(ripple::ttPAYMENT);

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
