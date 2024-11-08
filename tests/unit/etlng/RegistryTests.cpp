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

#include "etlng/Models.hpp"
#include "etlng/impl/Registry.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/StringUtils.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace etlng::impl;

namespace compiletime::checks {

struct Ext1 {
    static void
    onLedgerData(etlng::model::LedgerData const&);
};

struct Ext2 {
    static void
    onInitialObjects(uint32_t, std::vector<etlng::model::Object> const&, std::string);
};

struct Ext3 {
    static void
    onInitialData(etlng::model::LedgerData const&);
};

struct Ext4SpecMissing {
    static void
    onTransaction(uint32_t, etlng::model::Transaction const&);
};

struct Ext4Fixed {
    using spec = etlng::model::Spec<ripple::TxType::ttNFTOKEN_BURN>;

    static void
    onTransaction(uint32_t, etlng::model::Transaction const&);
};

struct Ext5 {
    static void
    onInitialObject(uint32_t, etlng::model::Object const&);
};

struct Ext6SpecMissing {
    static void
    onInitialTransaction(uint32_t, etlng::model::Transaction const&);
};

struct Ext6Fixed {
    using spec = etlng::model::Spec<ripple::TxType::ttNFTOKEN_BURN>;

    static void
    onInitialTransaction(uint32_t, etlng::model::Transaction const&);
};

struct ExtRealistic {
    using spec = etlng::model::Spec<
        ripple::TxType::ttNFTOKEN_BURN,
        ripple::TxType::ttNFTOKEN_ACCEPT_OFFER,
        ripple::TxType::ttNFTOKEN_CREATE_OFFER,
        ripple::TxType::ttNFTOKEN_CANCEL_OFFER,
        ripple::TxType::ttNFTOKEN_MINT>;

    static void
    onLedgerData(etlng::model::LedgerData const&);
    static void
    onInitialObject(uint32_t, etlng::model::Object const&);
    static void
    onInitialTransaction(uint32_t, etlng::model::Transaction const&);
};

struct ExtCombinesTwoOfKind : Ext2, Ext5 {};
struct ExtEmpty {};

// check all expectations of an extension are met
static_assert(not SomeExtension<ExtEmpty>);
static_assert(SomeExtension<Ext1>);
static_assert(SomeExtension<Ext2>);
static_assert(SomeExtension<Ext3>);
static_assert(not SomeExtension<Ext4SpecMissing>);
static_assert(SomeExtension<Ext4Fixed>);
static_assert(SomeExtension<Ext5>);
static_assert(not SomeExtension<Ext6SpecMissing>);
static_assert(SomeExtension<Ext6Fixed>);
static_assert(SomeExtension<ExtRealistic>);
static_assert(not SomeExtension<ExtCombinesTwoOfKind>);

struct ValidSpec {
    using spec = etlng::model::Spec<ripple::ttNFTOKEN_BURN, ripple::ttNFTOKEN_MINT>;
};

// invalid spec does not compile:
// struct DuplicatesSpec {
//     using spec = etlng::model::Spec<ripple::ttNFTOKEN_BURN, ripple::ttNFTOKEN_BURN, ripple::ttNFTOKEN_MINT>;
// };

static_assert(ContainsSpec<ValidSpec>);

}  // namespace compiletime::checks

namespace {

auto constinit LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
auto constinit SEQ = 30;

struct MockExtLedgerData {
    MOCK_METHOD(void, onLedgerData, (etlng::model::LedgerData const&), (const));
};

struct MockExtInitialData {
    MOCK_METHOD(void, onInitialData, (etlng::model::LedgerData const&), (const));
};

struct MockExtOnObject {
    MOCK_METHOD(void, onObject, (uint32_t, etlng::model::Object const&), (const));
};

struct MockExtTransactionNftBurn {
    using spec = etlng::model::Spec<ripple::TxType::ttNFTOKEN_BURN>;
    MOCK_METHOD(void, onTransaction, (uint32_t, etlng::model::Transaction const&), (const));
};

struct MockExtTransactionNftOffer {
    using spec = etlng::model::Spec<
        ripple::TxType::ttNFTOKEN_CREATE_OFFER,
        ripple::TxType::ttNFTOKEN_CANCEL_OFFER,
        ripple::TxType::ttNFTOKEN_ACCEPT_OFFER>;
    MOCK_METHOD(void, onTransaction, (uint32_t, etlng::model::Transaction const&), (const));
};

struct MockExtInitialObject {
    MOCK_METHOD(void, onInitialObject, (uint32_t, etlng::model::Object const&), (const));
};

struct MockExtInitialObjects {
    MOCK_METHOD(void, onInitialObjects, (uint32_t, std::vector<etlng::model::Object> const&, std::string), (const));
};

struct MockExtNftBurn {
    using spec = etlng::model::Spec<ripple::TxType::ttNFTOKEN_BURN>;
    MOCK_METHOD(void, onInitialTransaction, (uint32_t, etlng::model::Transaction const&), (const));
};

struct MockExtNftOffer {
    using spec = etlng::model::Spec<
        ripple::TxType::ttNFTOKEN_CREATE_OFFER,
        ripple::TxType::ttNFTOKEN_CANCEL_OFFER,
        ripple::TxType::ttNFTOKEN_ACCEPT_OFFER>;
    MOCK_METHOD(void, onInitialTransaction, (uint32_t, etlng::model::Transaction const&), (const));
};

struct RegistryTest : NoLoggerFixture {
    static std::pair<ripple::STTx, ripple::TxMeta>
    CreateNftTxAndMeta()
    {
        ripple::uint256 hash;
        EXPECT_TRUE(hash.parseHex("6C7F69A6D25A13AC4A2E9145999F45D4674F939900017A96885FDC2757E9284E"));

        static constinit auto txnHex =
            "1200192200000008240011CC9B201B001F71D6202A0000000168400000"
            "000000000C7321ED475D1452031E8F9641AF1631519A58F7B8681E172E"
            "4838AA0E59408ADA1727DD74406960041F34F10E0CBB39444B4D4E577F"
            "C0B7E8D843D091C2917E96E7EE0E08B30C91413EC551A2B8A1D405E8BA"
            "34FE185D8B10C53B40928611F2DE3B746F0303751868747470733A2F2F"
            "677265677765697362726F642E636F6D81146203F49C21D5D6E022CB16"
            "DE3538F248662FC73C";

        static constinit auto txnMeta =
            "201C00000001F8E511005025001F71B3556ED9C9459001E4F4A9121F4E"
            "07AB6D14898A5BBEF13D85C25D743540DB59F3CF566203F49C21D5D6E0"
            "22CB16DE3538F248662FC73CFFFFFFFFFFFFFFFFFFFFFFFFE6FAEC5A00"
            "0800006203F49C21D5D6E022CB16DE3538F248662FC73C8962EFA00000"
            "0006751868747470733A2F2F677265677765697362726F642E636F6DE1"
            "EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C93E8B1"
            "C200000028751868747470733A2F2F677265677765697362726F642E63"
            "6F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C"
            "9808B6B90000001D751868747470733A2F2F677265677765697362726F"
            "642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F24866"
            "2FC73C9C28BBAC00000012751868747470733A2F2F6772656777656973"
            "62726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538"
            "F248662FC73CA048C0A300000007751868747470733A2F2F6772656777"
            "65697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16"
            "DE3538F248662FC73CAACE82C500000029751868747470733A2F2F6772"
            "65677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E0"
            "22CB16DE3538F248662FC73CAEEE87B80000001E751868747470733A2F"
            "2F677265677765697362726F642E636F6DE1EC5A000800006203F49C21"
            "D5D6E022CB16DE3538F248662FC73CB30E8CAF00000013751868747470"
            "733A2F2F677265677765697362726F642E636F6DE1EC5A000800006203"
            "F49C21D5D6E022CB16DE3538F248662FC73CB72E91A200000008751868"
            "747470733A2F2F677265677765697362726F642E636F6DE1EC5A000800"
            "006203F49C21D5D6E022CB16DE3538F248662FC73CC1B453C40000002A"
            "751868747470733A2F2F677265677765697362726F642E636F6DE1EC5A"
            "000800006203F49C21D5D6E022CB16DE3538F248662FC73CC5D458BB00"
            "00001F751868747470733A2F2F677265677765697362726F642E636F6D"
            "E1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CC9F4"
            "5DAE00000014751868747470733A2F2F677265677765697362726F642E"
            "636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC7"
            "3CCE1462A500000009751868747470733A2F2F67726567776569736272"
            "6F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248"
            "662FC73CD89A24C70000002B751868747470733A2F2F67726567776569"
            "7362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE35"
            "38F248662FC73CDCBA29BA00000020751868747470733A2F2F67726567"
            "7765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB"
            "16DE3538F248662FC73CE0DA2EB100000015751868747470733A2F2F67"
            "7265677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6"
            "E022CB16DE3538F248662FC73CE4FA33A40000000A751868747470733A"
            "2F2F677265677765697362726F642E636F6DE1EC5A000800006203F49C"
            "21D5D6E022CB16DE3538F248662FC73CF39FFABD000000217518687474"
            "70733A2F2F677265677765697362726F642E636F6DE1EC5A0008000062"
            "03F49C21D5D6E022CB16DE3538F248662FC73CF7BFFFB0000000167518"
            "68747470733A2F2F677265677765697362726F642E636F6DE1EC5A0008"
            "00006203F49C21D5D6E022CB16DE3538F248662FC73CFBE004A7000000"
            "0B751868747470733A2F2F677265677765697362726F642E636F6DE1F1"
            "E1E72200000000501A6203F49C21D5D6E022CB16DE3538F248662FC73C"
            "662FC73C8962EFA000000006FAEC5A000800006203F49C21D5D6E022CB"
            "16DE3538F248662FC73C8962EFA000000006751868747470733A2F2F67"
            "7265677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6"
            "E022CB16DE3538F248662FC73C93E8B1C200000028751868747470733A"
            "2F2F677265677765697362726F642E636F6DE1EC5A000800006203F49C"
            "21D5D6E022CB16DE3538F248662FC73C9808B6B90000001D7518687474"
            "70733A2F2F677265677765697362726F642E636F6DE1EC5A0008000062"
            "03F49C21D5D6E022CB16DE3538F248662FC73C9C28BBAC000000127518"
            "68747470733A2F2F677265677765697362726F642E636F6DE1EC5A0008"
            "00006203F49C21D5D6E022CB16DE3538F248662FC73CA048C0A3000000"
            "07751868747470733A2F2F677265677765697362726F642E636F6DE1EC"
            "5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CAACE82C5"
            "00000029751868747470733A2F2F677265677765697362726F642E636F"
            "6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CAE"
            "EE87B80000001E751868747470733A2F2F677265677765697362726F64"
            "2E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662F"
            "C73CB30E8CAF00000013751868747470733A2F2F677265677765697362"
            "726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F2"
            "48662FC73CB72E91A200000008751868747470733A2F2F677265677765"
            "697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE"
            "3538F248662FC73CC1B453C40000002A751868747470733A2F2F677265"
            "677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022"
            "CB16DE3538F248662FC73CC5D458BB0000001F751868747470733A2F2F"
            "677265677765697362726F642E636F6DE1EC5A000800006203F49C21D5"
            "D6E022CB16DE3538F248662FC73CC9F45DAE0000001475186874747073"
            "3A2F2F677265677765697362726F642E636F6DE1EC5A000800006203F4"
            "9C21D5D6E022CB16DE3538F248662FC73CCE1462A50000000975186874"
            "7470733A2F2F677265677765697362726F642E636F6DE1EC5A00080000"
            "6203F49C21D5D6E022CB16DE3538F248662FC73CD89A24C70000002B75"
            "1868747470733A2F2F677265677765697362726F642E636F6DE1EC5A00"
            "0800006203F49C21D5D6E022CB16DE3538F248662FC73CDCBA29BA0000"
            "0020751868747470733A2F2F677265677765697362726F642E636F6DE1"
            "EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CE0DA2E"
            "B100000015751868747470733A2F2F677265677765697362726F642E63"
            "6F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C"
            "E4FA33A40000000A751868747470733A2F2F677265677765697362726F"
            "642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F24866"
            "2FC73CEF7FF5C60000002C751868747470733A2F2F6772656777656973"
            "62726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538"
            "F248662FC73CF39FFABD00000021751868747470733A2F2F6772656777"
            "65697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16"
            "DE3538F248662FC73CF7BFFFB000000016751868747470733A2F2F6772"
            "65677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E0"
            "22CB16DE3538F248662FC73CFBE004A70000000B751868747470733A2F"
            "2F677265677765697362726F642E636F6DE1F1E1E1E511006125001F71"
            "B3556ED9C9459001E4F4A9121F4E07AB6D14898A5BBEF13D85C25D7435"
            "40DB59F3CF56BE121B82D5812149D633F605EB07265A80B762A365CE94"
            "883089FEEE4B955701E6240011CC9B202B0000002C6240000002540BE3"
            "ECE1E72200000000240011CC9C2D0000000A202B0000002D202C000000"
            "066240000002540BE3E081146203F49C21D5D6E022CB16DE3538F24866"
            "2FC73CE1E1F1031000";

        auto const metaBlob = hexStringToBinaryString(txnMeta);
        auto const txnBlob = hexStringToBinaryString(txnHex);

        ripple::SerialIter it{txnBlob.data(), txnBlob.size()};
        return {ripple::STTx{it}, ripple::TxMeta{hash, SEQ, metaBlob}};
    }

    static auto
    CreateTransaction(ripple::TxType type)
    {
        auto [sttx, meta] = CreateNftTxAndMeta();
        return etlng::model::Transaction{
            .raw = "",
            .metaRaw = "",
            .sttx = sttx,
            .meta = meta,
            .id = ripple::uint256{"0000000000000000000000000000000000000000000000000000000000000001"},
            .key = "0000000000000000000000000000000000000000000000000000000000000001",
            .type = type
        };
    }

    static auto
    CreateObject()
    {
        return etlng::model::Object{
            .key = {},
            .keyRaw = {},
            .data = {},
            .dataRaw = {},
            .successor = {},
            .predecessor = {},
            .type = {},
        };
    }
};

}  // namespace

TEST_F(RegistryTest, FilteringOfTxWorksCorrectlyForInitialTransaction)
{
    auto transactions = std::vector{
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto extBurn = MockExtNftBurn{};
    auto extOffer = MockExtNftOffer{};

    EXPECT_CALL(extBurn, onInitialTransaction(testing::_, testing::_)).Times(2);  // 2 burn txs
    EXPECT_CALL(extOffer, onInitialTransaction(testing::_, testing::_));          // 1 create offer

    auto const header = CreateLedgerHeader(LEDGERHASH, SEQ);
    auto reg = Registry<MockExtNftBurn&, MockExtNftOffer&>(extBurn, extOffer);
    reg.dispatchInitialData(etlng::model::LedgerData{
        .transactions = transactions,
        .objects = {},
        .successors = {},
        .header = header,
        .rawHeader = {},
        .seq = SEQ,
    });
}

TEST_F(RegistryTest, FilteringOfTxWorksCorrectlyForTransaction)
{
    auto transactions = std::vector{
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto extBurn = MockExtTransactionNftBurn{};
    auto extOffer = MockExtTransactionNftOffer{};

    EXPECT_CALL(extBurn, onTransaction(testing::_, testing::_)).Times(2);  // 2 burn txs
    EXPECT_CALL(extOffer, onTransaction(testing::_, testing::_));          // 1 create offer

    auto const header = CreateLedgerHeader(LEDGERHASH, SEQ);
    auto reg = Registry<MockExtTransactionNftBurn&, MockExtTransactionNftOffer&>(extBurn, extOffer);
    reg.dispatch(etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .header = header,
        .rawHeader = {},
        .seq = SEQ
    });
}

TEST_F(RegistryTest, InitialObjectsEmpty)
{
    auto extObj = MockExtInitialObject{};
    auto extObjs = MockExtInitialObjects{};

    EXPECT_CALL(extObj, onInitialObject(testing::_, testing::_)).Times(0);       // 0 empty objects sent
    EXPECT_CALL(extObjs, onInitialObjects(testing::_, testing::_, testing::_));  // 1 vector passed as is

    auto reg = Registry<MockExtInitialObject&, MockExtInitialObjects&>(extObj, extObjs);
    reg.dispatchInitialObjects(SEQ, {}, {});
}

TEST_F(RegistryTest, InitialObjectsDispatched)
{
    auto extObj = MockExtInitialObject{};
    auto extObjs = MockExtInitialObjects{};

    EXPECT_CALL(extObj, onInitialObject(testing::_, testing::_)).Times(3);       // 3 objects sent
    EXPECT_CALL(extObjs, onInitialObjects(testing::_, testing::_, testing::_));  // 1 vector passed as is

    auto reg = Registry<MockExtInitialObject&, MockExtInitialObjects&>(extObj, extObjs);
    reg.dispatchInitialObjects(SEQ, {CreateObject(), CreateObject(), CreateObject()}, {});
}

TEST_F(RegistryTest, ObjectsDispatched)
{
    auto extObj = MockExtOnObject{};

    EXPECT_CALL(extObj, onObject(testing::_, testing::_)).Times(3);  // 3 objects sent

    auto const header = CreateLedgerHeader(LEDGERHASH, SEQ);
    auto reg = Registry<MockExtOnObject&>(extObj);
    reg.dispatch(etlng::model::LedgerData{
        .transactions = {},
        .objects = {CreateObject(), CreateObject(), CreateObject()},
        .successors = {},
        .header = header,
        .rawHeader = {},
        .seq = SEQ
    });
}

TEST_F(RegistryTest, OnLedgerDataForBatch)
{
    auto transactions = std::vector{
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto ext = MockExtLedgerData{};

    EXPECT_CALL(ext, onLedgerData(testing::_));  // 1 batch (dispatch call)

    auto const header = CreateLedgerHeader(LEDGERHASH, SEQ);
    auto reg = Registry<MockExtLedgerData&>(ext);
    reg.dispatch(etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .header = header,
        .rawHeader = {},
        .seq = SEQ
    });
}

TEST_F(RegistryTest, InitialObjectsCorrectOrderOfHookCalls)
{
    auto extObjs = MockExtInitialObjects{};
    auto extObj = MockExtInitialObject{};

    testing::InSequence const seqGuard;
    EXPECT_CALL(extObjs, onInitialObjects);
    EXPECT_CALL(extObj, onInitialObject).Times(3);

    auto reg = Registry<MockExtInitialObject&, MockExtInitialObjects&>(extObj, extObjs);
    reg.dispatchInitialObjects(SEQ, {CreateObject(), CreateObject(), CreateObject()}, {});
}

TEST_F(RegistryTest, InitialDataCorrectOrderOfHookCalls)
{
    auto extInitialData = MockExtInitialData{};
    auto extInitialTransaction = MockExtNftBurn{};

    auto transactions = std::vector{
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    testing::InSequence const seqGuard;
    EXPECT_CALL(extInitialData, onInitialData);
    EXPECT_CALL(extInitialTransaction, onInitialTransaction).Times(2);

    auto const header = CreateLedgerHeader(LEDGERHASH, SEQ);
    auto reg = Registry<MockExtNftBurn&, MockExtInitialData&>(extInitialTransaction, extInitialData);
    reg.dispatchInitialData(etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .header = header,
        .rawHeader = {},
        .seq = SEQ
    });
}

TEST_F(RegistryTest, LedgerDataCorrectOrderOfHookCalls)
{
    auto extLedgerData = MockExtLedgerData{};
    auto extOnTransaction = MockExtTransactionNftBurn{};
    auto extOnObject = MockExtOnObject{};

    auto transactions = std::vector{
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };
    auto objects = std::vector{
        CreateObject(),
        CreateObject(),
        CreateObject(),
    };

    // testing::Sequence seq;
    testing::InSequence const seqGuard;
    EXPECT_CALL(extLedgerData, onLedgerData);
    EXPECT_CALL(extOnTransaction, onTransaction).Times(2);
    EXPECT_CALL(extOnObject, onObject).Times(3);

    auto const header = CreateLedgerHeader(LEDGERHASH, SEQ);
    auto reg = Registry<MockExtOnObject&, MockExtTransactionNftBurn&, MockExtLedgerData&>(
        extOnObject, extOnTransaction, extLedgerData
    );
    reg.dispatch(etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = std::move(objects),
        .successors = {},
        .header = header,
        .rawHeader = {},
        .seq = SEQ
    });
}
