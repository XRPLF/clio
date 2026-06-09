#include "data/DBHelpers.hpp"
#include "etl/Models.hpp"
#include "etl/impl/ext/MPT.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <algorithm>
#include <string>
#include <string_view>
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

auto
createTransactionFromObjects(
    ripple::STObject const& txObj,
    ripple::STObject const& metaObj,
    ripple::TxType type
)
{
    auto const txBlob = txObj.getSerializer().peekData();
    auto const metaBlob = metaObj.getSerializer().peekData();

    ripple::SerialIter txIter{txBlob.data(), txBlob.size()};
    ripple::uint256 const id{kHash};

    return etl::model::Transaction{
        .raw = "",
        .metaRaw = "",
        .sttx = ripple::STTx{txIter},
        .meta = ripple::TxMeta{id, kSeq, metaBlob},
        .id = id,
        .key = std::string{kHash},
        .type = type
    };
}

ripple::STObject
createNewMPTokenNode(std::string_view holder)
{
    ripple::STObject newFields(ripple::sfNewFields);
    newFields.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN);
    newFields[ripple::sfMPTokenIssuanceID] = ripple::uint192{kMptIssuanceID};
    newFields.setAccountID(ripple::sfAccount, getAccountIdWithString(holder));

    ripple::STObject createdNode(ripple::sfCreatedNode);
    createdNode.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN);
    createdNode.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{});
    createdNode.emplace_back(std::move(newFields));
    return createdNode;
}

ripple::STObject
createPaymentMetaWithNewMPTokens(ripple::TER result = ripple::tesSUCCESS)
{
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldU8(ripple::sfTransactionResult, TERtoInt(result));
    metaObj.setFieldU32(ripple::sfTransactionIndex, 0);

    ripple::STArray affectedNodes(ripple::sfAffectedNodes);
    affectedNodes.push_back(createNewMPTokenNode(kHolderAccount));
    affectedNodes.push_back(createNewMPTokenNode(kHolderAccount2));
    metaObj.setFieldArray(ripple::sfAffectedNodes, affectedNodes);

    return metaObj;
}

auto
createPaymentWithMultipleHoldersTestData(ripple::TER result = ripple::tesSUCCESS)
{
    auto transactions = std::vector{createTransactionFromObjects(
        createPaymentTransactionObject(kHolderAccount, kHolderAccount2, 1, 1, 1),
        createPaymentMetaWithNewMPTokens(result),
        ripple::TxType::ttPAYMENT
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
            ripple::TxType::ttMPTOKEN_ISSUANCE_CREATE
        ),  // metadata does not create an MPT holder
        util::createTransaction(ripple::TxType::ttAMM_CREATE),  // metadata is not MPT
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
    auto transactions = std::vector{
        util::createTransaction(
            ripple::TxType::ttMPTOKEN_ISSUANCE_CREATE
        ),  // metadata does not create an MPT holder
        util::createTransaction(ripple::TxType::ttMPTOKEN_AUTHORIZE, kHash, kTxnMeta, kTxnHex),
        util::createTransaction(ripple::TxType::ttAMM_CREATE),  // metadata is not MPT
        util::createTransaction(
            ripple::TxType::ttMPTOKEN_ISSUANCE_CREATE
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

auto
createMultipleHoldersTestData()
{
    auto transactions = std::vector{
        util::createTransaction(ripple::TxType::ttMPTOKEN_AUTHORIZE, kHash, kTxnMeta, kTxnHex),
        util::createTransaction(ripple::TxType::ttMPTOKEN_AUTHORIZE, kHash2, kTxnMeta, kTxnHex),
        util::createTransaction(ripple::TxType::ttMPTOKEN_AUTHORIZE, kHash3, kTxnMeta, kTxnHex)
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

    ext_.onLedgerData(data);
}

TEST_F(MPTExtTests, OnInitialDataFiltersAndWritesMPTs)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, writeMPTHolders).WillOnce([](auto const& holders) {
        EXPECT_EQ(holders.size(), 1);  // Only metadata creating an MPToken is written
    });

    ext_.onInitialData(data);
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
        EXPECT_EQ(holders.size(), 3);  // Expect all three AUTHORIZE transactions

        auto const expectedAccount =
            rpc::accountFromStringStrict(kHolderAccount);  // Expect all three to be the same
        EXPECT_TRUE(std::ranges::all_of(holders, [&expectedAccount](auto const& data) {
            return data.holder == expectedAccount;
        }));
    });

    ext_.onInitialData(data);
}

TEST_F(MPTExtTests, OnInitialDataDoesNotWriteFailedMPTokenCreations)
{
    auto const data = createPaymentWithMultipleHoldersTestData(ripple::tecINCOMPLETE);

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
    auto const expectedMptID = ripple::uint192{kMptIssuanceID};
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
