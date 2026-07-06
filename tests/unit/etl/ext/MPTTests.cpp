#include "data/DBHelpers.hpp"
#include "etl/Models.hpp"
#include "etl/impl/ext/MPT.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MPTokenTestObjects.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace etl;
using namespace etl::impl;
using namespace data;
using namespace testing;

namespace {

constinit auto const kSeq = 123u;
constinit auto const kLedgerHash =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kHolderAccount = "rK1EX542EgA9m948JrJRaEzwLVEhqWvnr9";
constinit auto const kHolderAccount2 = "rnd1nHuzceyQDqnLH8urWNr4QBKt4v7WVk";
constinit auto const kMptIssuanceID = "000004C463C52827307480341125DA0577DEFC38405B0E3E";

constinit auto const kTxnHex =
    "120039220000000024002DBD1A201B002DBDA36840000000000000017321EDECF25C029811CAD07AFD616EB75E3803"
    "E44D0D59A6826AC25FE3"
    "4A43626D2D157440244262E760314164843026CE2F100D0BFEB0DD6F75026FEB3F75FCAA943F5C874FF0411BC82A85"
    "DE504B434B5EC3C6A692"
    "3CC37A1C2ABD3E98EFFC8240B9D0018114CEF330DB51154D8DEE249CC3D6DFD04B91F648EE0115002DBD1817E0AF9F"
    "DE4F9978B8FCD8A50636"
    "30B5737DA605";

constinit auto const kTxnMeta =
    "201C00000002F8E311007F562668E165750018E0AE5808C131BAF4C26441D2BCF76F8628774DFDF098B7250BE88114"
    "CEF330DB51154D8DEE24"
    "9CC3D6DFD04B91F648EE0115002DBD1817E0AF9FDE4F9978B8FCD8A5063630B5737DA605E1E1E511006425002DBD2F"
    "55E85C182A243C7CBF0E"
    "F7B8B3E0C8AE68E3DE6616DE1EFE168CD913CA6520444D568F18252475DFAC9D5DE5423DFA08842F398F346DEB2BD5"
    "46C526D26BF81E345CE7"
    "2200000000588F18252475DFAC9D5DE5423DFA08842F398F346DEB2BD546C526D26BF81E345C8214CEF330DB51154D"
    "8DEE249CC3D6DFD04B91"
    "F648EEE1E1E511006125002DBD2F55E85C182A243C7CBF0EF7B8B3E0C8AE68E3DE6616DE1EFE168CD913CA6520444D"
    "56F7D3073515F1C71F2A"
    "D00941BA714A3FBE3D91AEAFCD6345B5389004AD707E95E624002DBD1A2D00000001624000000005F5E0FFE1E72200"
    "00000024002DBD1B2D00"
    "000002624000000005F5E0FE8114CEF330DB51154D8DEE249CC3D6DFD04B91F648EEE1E1F1031000";

constinit auto const kHash = "6005B465CBBF7FA8E41AC0C0CD38491026D9411FCB7BA46E2AEBB3AF7654261B";
constinit auto const kHash2 = "6005B465CBBF7FA8E41AC0C0CD38491026D9411FCB7BA46E2AEBB3AF7654261C";
constinit auto const kHash3 = "6005B465CBBF7FA8E41AC0C0CD38491026D9411FCB7BA46E2AEBB3AF7654261D";

// The issuance ID carried by the ltMPTOKEN CreatedNode in kTxnMeta.
constinit auto const kIssuanceID = "002DBD1817E0AF9FDE4F9978B8FCD8A5063630B5737DA605";

constinit auto const kAccount = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
constinit auto const kAccount2 = "rnd1nHuzceyQDqnLH8urWNr4QBKt4v7WVk";
constexpr auto kHighFanoutAccountCount = 1001u;

xrpl::AccountID
accountIDFromSeed(std::uint32_t seed)
{
    std::array<unsigned char, xrpl::AccountID::size()> bytes{};
    bytes[16] = static_cast<unsigned char>(seed >> 24);
    bytes[17] = static_cast<unsigned char>(seed >> 16);
    bytes[18] = static_cast<unsigned char>(seed >> 8);
    bytes[19] = static_cast<unsigned char>(seed);
    return xrpl::AccountID::fromVoid(bytes.data());
}

xrpl::STObject
createAccountRootNode(xrpl::AccountID const& account)
{
    xrpl::STObject fields(xrpl::sfFinalFields);
    fields.setAccountID(xrpl::sfAccount, account);

    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltACCOUNT_ROOT);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    node.set(std::move(fields));
    return node;
}

// One Payment transaction touching two distinct issuances with three affected accounts.
etl::model::Transaction
createMultiIssuanceTransaction()
{
    xrpl::Slice const slice("test", 4);
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttPAYMENT);
    tx.setAccountID(xrpl::sfAccount, getAccountIdWithString(kAccount));
    tx.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(100, false));
    tx.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    tx.setAccountID(xrpl::sfDestination, getAccountIdWithString(kAccount2));
    tx.setFieldU32(xrpl::sfSequence, 1);
    tx.setFieldVL(xrpl::sfSigningPubKey, slice);

    auto const serialized = tx.getSerializer();
    auto const sttx = xrpl::STTx{xrpl::SerialIter{serialized.slice()}};

    auto const issuanceA = xrpl::makeMptID(1, getAccountIdWithString(kHolderAccount));
    auto const issuanceB = xrpl::makeMptID(2, getAccountIdWithString(kHolderAccount));

    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    xrpl::STArray affectedNodes(xrpl::sfAffectedNodes);
    affectedNodes.push_back(util::createMPTokenNode(xrpl::sfModifiedNode, issuanceA, kAccount));
    affectedNodes.push_back(util::createMPTokenNode(xrpl::sfModifiedNode, issuanceB, kAccount2));
    affectedNodes.push_back(
        util::createMPTokenIssuanceNode(xrpl::sfModifiedNode, 1, kHolderAccount)
    );  // issuanceA again
    metaObj.setFieldArray(xrpl::sfAffectedNodes, affectedNodes);

    auto const txMeta =
        xrpl::TxMeta{sttx.getTransactionID(), kSeq, metaObj.getSerializer().peekData()};

    return etl::model::Transaction{
        .raw = "",
        .metaRaw = "",
        .sttx = sttx,
        .meta = txMeta,
        .id = sttx.getTransactionID(),
        .key = "0000000000000000000000000000000000000000000000000000000000000002",
        .type = sttx.getTxnType()
    };
}

etl::model::Transaction
createHighFanoutIssuanceTransaction()
{
    xrpl::Slice const slice("test", 4);
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttPAYMENT);
    tx.setAccountID(xrpl::sfAccount, getAccountIdWithString(kAccount));
    tx.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(100, false));
    tx.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    tx.setAccountID(xrpl::sfDestination, getAccountIdWithString(kAccount2));
    tx.setFieldU32(xrpl::sfSequence, 1);
    tx.setFieldVL(xrpl::sfSigningPubKey, slice);

    auto const serialized = tx.getSerializer();
    auto const sttx = xrpl::STTx{xrpl::SerialIter{serialized.slice()}};

    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    xrpl::STArray affectedNodes(xrpl::sfAffectedNodes);
    affectedNodes.push_back(
        util::createMPTokenNode(xrpl::sfModifiedNode, xrpl::uint192{kMptIssuanceID}, kHolderAccount)
    );
    for (std::uint32_t i = 0; i < kHighFanoutAccountCount; ++i)
        affectedNodes.push_back(createAccountRootNode(accountIDFromSeed(i + 1)));
    metaObj.setFieldArray(xrpl::sfAffectedNodes, affectedNodes);

    auto const txMeta =
        xrpl::TxMeta{sttx.getTransactionID(), kSeq, metaObj.getSerializer().peekData()};

    return etl::model::Transaction{
        .raw = "",
        .metaRaw = "",
        .sttx = sttx,
        .meta = txMeta,
        .id = sttx.getTransactionID(),
        .key = "0000000000000000000000000000000000000000000000000000000000000003",
        .type = sttx.getTxnType()
    };
}

auto
createTransactionFromObjects(
    xrpl::STObject const& txObj,
    xrpl::STObject const& metaObj,
    xrpl::TxType type
)
{
    auto const txBlob = txObj.getSerializer().peekData();
    auto const metaBlob = metaObj.getSerializer().peekData();

    xrpl::SerialIter txIter{txBlob.data(), txBlob.size()};
    xrpl::uint256 const id{kHash};

    return etl::model::Transaction{
        .raw = "",
        .metaRaw = "",
        .sttx = xrpl::STTx{txIter},
        .meta = xrpl::TxMeta{id, kSeq, metaBlob},
        .id = id,
        .key = std::string{kHash},
        .type = type
    };
}

xrpl::STObject
createPaymentMetaWithNewMPTokens(xrpl::TER result = xrpl::tesSUCCESS)
{
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldU8(xrpl::sfTransactionResult, TERtoInt(result));
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 0);

    xrpl::STArray affectedNodes(xrpl::sfAffectedNodes);
    affectedNodes.push_back(
        util::createMPTokenNode(xrpl::sfCreatedNode, xrpl::uint192{kMptIssuanceID}, kHolderAccount)
    );
    affectedNodes.push_back(
        util::createMPTokenNode(xrpl::sfCreatedNode, xrpl::uint192{kMptIssuanceID}, kHolderAccount2)
    );
    metaObj.setFieldArray(xrpl::sfAffectedNodes, affectedNodes);

    return metaObj;
}

auto
createPaymentWithMultipleHoldersTestData(xrpl::TER result = xrpl::tesSUCCESS)
{
    auto transactions = std::vector{createTransactionFromObjects(
        createPaymentTransactionObject(kHolderAccount, kHolderAccount2, 1, 1, 1),
        createPaymentMetaWithNewMPTokens(result),
        xrpl::TxType::ttPAYMENT
    )};

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    return etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSeq
    };
}

auto
createTestDataWithoutMPToken()
{
    auto transactions = std::vector{
        util::createTransaction(
            xrpl::TxType::ttMPTOKEN_ISSUANCE_CREATE
        ),  // metadata does not create an MPT holder
        util::createTransaction(xrpl::TxType::ttAMM_CREATE),  // metadata is not MPT
    };

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    return etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSeq
    };
}

auto
createTestData()
{
    // Only the AUTHORIZE transaction carries metadata with MPT affected nodes.
    auto transactions = std::vector{
        util::createTransaction(
            xrpl::TxType::ttMPTOKEN_ISSUANCE_CREATE
        ),  // metadata does not create an MPT holder
        util::createTransaction(xrpl::TxType::ttMPTOKEN_AUTHORIZE, kHash, kTxnMeta, kTxnHex),
        util::createTransaction(xrpl::TxType::ttAMM_CREATE),  // metadata is not MPT
        util::createTransaction(
            xrpl::TxType::ttMPTOKEN_ISSUANCE_CREATE
        ),  // metadata does not create an MPT holder
    };

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    return etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSeq
    };
}

// Same AUTHORIZE fixture as kTxnMeta, with a distinct transaction index.
etl::model::Transaction
createAuthorizeTransactionWithIndex(std::string const& hashStr, std::uint32_t txIndex)
{
    auto tx =
        util::createTransaction(xrpl::TxType::ttMPTOKEN_AUTHORIZE, hashStr, kTxnMeta, kTxnHex);

    auto const metaBlob = xrpl::strUnHex(kTxnMeta);
    EXPECT_TRUE(metaBlob.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    xrpl::SerialIter sitMeta{xrpl::makeSlice(*metaBlob)};
    xrpl::STObject metaObj{sitMeta, xrpl::sfMetadata};
    metaObj.setFieldU32(xrpl::sfTransactionIndex, txIndex);

    xrpl::uint256 hash;
    EXPECT_TRUE(hash.parseHex(hashStr));
    tx.meta = xrpl::TxMeta{hash, kSeq, metaObj.getSerializer().peekData()};
    return tx;
}

auto
createMultipleHoldersTestData()
{
    auto transactions = std::vector{
        createAuthorizeTransactionWithIndex(kHash, 0),
        createAuthorizeTransactionWithIndex(kHash2, 1),
        createAuthorizeTransactionWithIndex(kHash3, 2)
    };

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    return etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSeq
    };
}

}  // namespace

struct MPTExtTests : util::prometheus::WithPrometheus, MockBackendTest {
protected:
    MPTExt ext_{backend_};
};

TEST_F(MPTExtTests, OnLedgerDataFiltersAndWritesMPTs)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 1);  // Only metadata creating an MPToken is written
    });

    std::vector<MPTokenIssuanceTransactionsData> issuanceTxs;
    std::vector<MPTokenIssuanceTransactionsData> accountIssuanceTxs;
    EXPECT_CALL(*backend_, writeMPTokenIssuanceTransactions).WillOnce(SaveArg<0>(&issuanceTxs));
    EXPECT_CALL(*backend_, writeAccountMPTokenIssuanceTransactions)
        .WillOnce(SaveArg<0>(&accountIssuanceTxs));

    ext_.onLedgerData(data);

    // Only the AUTHORIZE fixture touches an MPT object.
    ASSERT_EQ(issuanceTxs.size(), 1);
    EXPECT_EQ(issuanceTxs[0].mptIssuanceID, xrpl::uint192(kIssuanceID));
    EXPECT_FALSE(issuanceTxs[0].accounts.empty());
    EXPECT_TRUE(issuanceTxs[0].accounts.contains(getAccountIdWithString(kHolderAccount)));
    EXPECT_EQ(issuanceTxs, accountIssuanceTxs);  // same vector goes to both tables
}

TEST_F(MPTExtTests, OnInitialDataFiltersAndWritesMPTs)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 1);  // Only metadata creating an MPToken is written
    });

    std::vector<MPTokenIssuanceTransactionsData> issuanceTxs;
    std::vector<MPTokenIssuanceTransactionsData> accountIssuanceTxs;
    EXPECT_CALL(*backend_, writeMPTokenIssuanceTransactions).WillOnce(SaveArg<0>(&issuanceTxs));
    EXPECT_CALL(*backend_, writeAccountMPTokenIssuanceTransactions)
        .WillOnce(SaveArg<0>(&accountIssuanceTxs));

    ext_.onInitialData(data);

    // Only the AUTHORIZE fixture touches an MPT object.
    ASSERT_EQ(issuanceTxs.size(), 1);
    EXPECT_EQ(issuanceTxs[0].mptIssuanceID, xrpl::uint192(kIssuanceID));
    EXPECT_FALSE(issuanceTxs[0].accounts.empty());
    EXPECT_TRUE(issuanceTxs[0].accounts.contains(getAccountIdWithString(kHolderAccount)));
    EXPECT_EQ(issuanceTxs, accountIssuanceTxs);  // same vector goes to both tables
}

TEST_F(MPTExtTests, OnInitialObjectWritesMPT)
{
    auto const data = util::createObjectWithMPT();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 1);
    });

    ext_.onInitialObject(kSeq, data);
}

TEST_F(MPTExtTests, OnInitialDataWithMultipleHolders)
{
    auto const data = createMultipleHoldersTestData();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 3);  // All three AUTHORIZE transactions

        auto const expectedAccount =
            rpc::accountFromStringStrict(kHolderAccount);  // Same holder in each fixture
        EXPECT_TRUE(std::ranges::all_of(holders, [&expectedAccount](auto const& data) {
            return data.holder == expectedAccount;
        }));
    });

    std::vector<MPTokenIssuanceTransactionsData> issuanceTxs;
    std::vector<MPTokenIssuanceTransactionsData> accountIssuanceTxs;
    EXPECT_CALL(*backend_, writeMPTokenIssuanceTransactions).WillOnce(SaveArg<0>(&issuanceTxs));
    EXPECT_CALL(*backend_, writeAccountMPTokenIssuanceTransactions)
        .WillOnce(SaveArg<0>(&accountIssuanceTxs));

    ext_.onInitialData(data);

    // One record per AUTHORIZE transaction; each has a distinct transaction index.
    ASSERT_EQ(issuanceTxs.size(), 3);
    EXPECT_TRUE(std::ranges::all_of(issuanceTxs, [](auto const& record) {
        return record.mptIssuanceID == xrpl::uint192(kIssuanceID);
    }));
    std::vector<std::uint32_t> indices;
    std::ranges::transform(issuanceTxs, std::back_inserter(indices), [](auto const& record) {
        return record.transactionIndex;
    });
    EXPECT_THAT(indices, UnorderedElementsAre(0, 1, 2));
    EXPECT_EQ(issuanceTxs, accountIssuanceTxs);
}

TEST_F(MPTExtTests, NoMPTTransactionsWritesNothing)
{
    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttAMM_CREATE),
        util::createTransaction(xrpl::TxType::ttAMM_CREATE)
    };

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto const data = etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSeq
    };

    EXPECT_CALL(*backend_, writeMPTHolders).Times(0);
    EXPECT_CALL(*backend_, writeMPTokenIssuanceTransactions).Times(0);
    EXPECT_CALL(*backend_, writeAccountMPTokenIssuanceTransactions).Times(0);

    ext_.onLedgerData(data);
}

TEST_F(MPTExtTests, OnLedgerDataDedupsMultiIssuanceFanout)
{
    auto transactions = std::vector{createMultiIssuanceTransaction()};

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto const data = etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSeq
    };

    std::vector<MPTokenIssuanceTransactionsData> issuanceTxs;
    std::vector<MPTokenIssuanceTransactionsData> accountIssuanceTxs;
    EXPECT_CALL(*backend_, writeMPTokenIssuanceTransactions).WillOnce(SaveArg<0>(&issuanceTxs));
    EXPECT_CALL(*backend_, writeAccountMPTokenIssuanceTransactions)
        .WillOnce(SaveArg<0>(&accountIssuanceTxs));

    ext_.onLedgerData(data);

    // issuanceA is touched twice, issuanceB once; each record carries the full account set.
    auto const issuanceA = xrpl::makeMptID(1, getAccountIdWithString(kHolderAccount));
    auto const issuanceB = xrpl::makeMptID(2, getAccountIdWithString(kHolderAccount));

    ASSERT_EQ(issuanceTxs.size(), 2);
    EXPECT_EQ(issuanceTxs[0].mptIssuanceID, issuanceA);
    EXPECT_EQ(issuanceTxs[1].mptIssuanceID, issuanceB);
    for (auto const& record : issuanceTxs) {
        EXPECT_EQ(record.accounts.size(), 3);
        EXPECT_TRUE(record.accounts.contains(getAccountIdWithString(kAccount)));
        EXPECT_TRUE(record.accounts.contains(getAccountIdWithString(kAccount2)));
        EXPECT_TRUE(record.accounts.contains(getAccountIdWithString(kHolderAccount)));
        EXPECT_EQ(record.ledgerSequence, kSeq);
    }
    EXPECT_EQ(issuanceTxs, accountIssuanceTxs);
}

TEST_F(MPTExtTests, OnLedgerDataWritesHighFanoutIssuanceIndexWithoutHolders)
{
    auto transactions = std::vector{createHighFanoutIssuanceTransaction()};

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto const data = etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSeq
    };

    EXPECT_CALL(*backend_, writeMPTHolders).Times(0);

    std::vector<MPTokenIssuanceTransactionsData> issuanceTxs;
    std::vector<MPTokenIssuanceTransactionsData> accountIssuanceTxs;
    EXPECT_CALL(*backend_, writeMPTokenIssuanceTransactions).WillOnce(SaveArg<0>(&issuanceTxs));
    EXPECT_CALL(*backend_, writeAccountMPTokenIssuanceTransactions)
        .WillOnce(SaveArg<0>(&accountIssuanceTxs));

    ext_.onLedgerData(data);

    ASSERT_EQ(issuanceTxs.size(), 1);
    EXPECT_EQ(issuanceTxs[0].mptIssuanceID, xrpl::uint192{kMptIssuanceID});
    EXPECT_GT(issuanceTxs[0].accounts.size(), kHighFanoutAccountCount);
    EXPECT_EQ(issuanceTxs[0].ledgerSequence, kSeq);
    EXPECT_EQ(issuanceTxs, accountIssuanceTxs);
}

TEST_F(MPTExtTests, OnInitialDataDoesNotWriteFailedMPTokenCreations)
{
    auto const data = createPaymentWithMultipleHoldersTestData(xrpl::tecINCOMPLETE);

    EXPECT_CALL(*backend_, writeMPTHolders).Times(0);

    ext_.onInitialData(data);
}

TEST_F(MPTExtTests, OnInitialDataDoesNotWriteWithoutCreatedMPToken)
{
    auto const data = createTestDataWithoutMPToken();

    EXPECT_CALL(*backend_, writeMPTHolders).Times(0);

    ext_.onInitialData(data);
}

TEST_F(MPTExtTests, OnInitialDataWritesAllMPTsCreatedByPayment)
{
    auto const data = createPaymentWithMultipleHoldersTestData();
    auto const expectedMptID = xrpl::uint192{kMptIssuanceID};
    auto const expectedAccount = getAccountIdWithString(kHolderAccount);
    auto const expectedAccount2 = getAccountIdWithString(kHolderAccount2);

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([&](auto const& holders) {
        EXPECT_THAT(
            holders,
            UnorderedElementsAre(
                AllOf(
                    Field(&MPTHolderData::mptID, expectedMptID),
                    Field(&MPTHolderData::holder, expectedAccount)
                ),
                AllOf(
                    Field(&MPTHolderData::mptID, expectedMptID),
                    Field(&MPTHolderData::holder, expectedAccount2)
                )
            )
        );
    });

    ext_.onInitialData(data);
}
