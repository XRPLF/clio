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
#include "util/BinaryTestObject.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/TxFormats.h>

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

constinit auto const LedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const Seq = 30;

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

struct RegistryTest : NoLoggerFixture {};

}  // namespace

TEST_F(RegistryTest, FilteringOfTxWorksCorrectlyForInitialTransaction)
{
    auto transactions = std::vector{
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto extBurn = MockExtNftBurn{};
    auto extOffer = MockExtNftOffer{};

    EXPECT_CALL(extBurn, onInitialTransaction(testing::_, testing::_)).Times(2);  // 2 burn txs
    EXPECT_CALL(extOffer, onInitialTransaction(testing::_, testing::_));          // 1 create offer

    auto const header = CreateLedgerHeader(LedgerHash, Seq);
    auto reg = Registry<MockExtNftBurn&, MockExtNftOffer&>(extBurn, extOffer);
    reg.dispatchInitialData(etlng::model::LedgerData{
        .transactions = transactions,
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = Seq,
    });
}

TEST_F(RegistryTest, FilteringOfTxWorksCorrectlyForTransaction)
{
    auto transactions = std::vector{
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto extBurn = MockExtTransactionNftBurn{};
    auto extOffer = MockExtTransactionNftOffer{};

    EXPECT_CALL(extBurn, onTransaction(testing::_, testing::_)).Times(2);  // 2 burn txs
    EXPECT_CALL(extOffer, onTransaction(testing::_, testing::_));          // 1 create offer

    auto const header = CreateLedgerHeader(LedgerHash, Seq);
    auto reg = Registry<MockExtTransactionNftBurn&, MockExtTransactionNftOffer&>(extBurn, extOffer);
    reg.dispatch(etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = Seq
    });
}

TEST_F(RegistryTest, InitialObjectsEmpty)
{
    auto extObj = MockExtInitialObject{};
    auto extObjs = MockExtInitialObjects{};

    EXPECT_CALL(extObj, onInitialObject(testing::_, testing::_)).Times(0);       // 0 empty objects sent
    EXPECT_CALL(extObjs, onInitialObjects(testing::_, testing::_, testing::_));  // 1 vector passed as is

    auto reg = Registry<MockExtInitialObject&, MockExtInitialObjects&>(extObj, extObjs);
    reg.dispatchInitialObjects(Seq, {}, {});
}

TEST_F(RegistryTest, InitialObjectsDispatched)
{
    auto extObj = MockExtInitialObject{};
    auto extObjs = MockExtInitialObjects{};

    EXPECT_CALL(extObj, onInitialObject(testing::_, testing::_)).Times(3);       // 3 objects sent
    EXPECT_CALL(extObjs, onInitialObjects(testing::_, testing::_, testing::_));  // 1 vector passed as is

    auto reg = Registry<MockExtInitialObject&, MockExtInitialObjects&>(extObj, extObjs);
    reg.dispatchInitialObjects(Seq, {util::CreateObject(), util::CreateObject(), util::CreateObject()}, {});
}

TEST_F(RegistryTest, ObjectsDispatched)
{
    auto extObj = MockExtOnObject{};

    EXPECT_CALL(extObj, onObject(testing::_, testing::_)).Times(3);  // 3 objects sent

    auto const header = CreateLedgerHeader(LedgerHash, Seq);
    auto reg = Registry<MockExtOnObject&>(extObj);
    reg.dispatch(etlng::model::LedgerData{
        .transactions = {},
        .objects = {util::CreateObject(), util::CreateObject(), util::CreateObject()},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = Seq
    });
}

TEST_F(RegistryTest, OnLedgerDataForBatch)
{
    auto transactions = std::vector{
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto ext = MockExtLedgerData{};

    EXPECT_CALL(ext, onLedgerData(testing::_));  // 1 batch (dispatch call)

    auto const header = CreateLedgerHeader(LedgerHash, Seq);
    auto reg = Registry<MockExtLedgerData&>(ext);
    reg.dispatch(etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = Seq
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
    reg.dispatchInitialObjects(Seq, {util::CreateObject(), util::CreateObject(), util::CreateObject()}, {});
}

TEST_F(RegistryTest, InitialDataCorrectOrderOfHookCalls)
{
    auto extInitialData = MockExtInitialData{};
    auto extInitialTransaction = MockExtNftBurn{};

    auto transactions = std::vector{
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    testing::InSequence const seqGuard;
    EXPECT_CALL(extInitialData, onInitialData);
    EXPECT_CALL(extInitialTransaction, onInitialTransaction).Times(2);

    auto const header = CreateLedgerHeader(LedgerHash, Seq);
    auto reg = Registry<MockExtNftBurn&, MockExtInitialData&>(extInitialTransaction, extInitialData);
    reg.dispatchInitialData(etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = Seq
    });
}

TEST_F(RegistryTest, LedgerDataCorrectOrderOfHookCalls)
{
    auto extLedgerData = MockExtLedgerData{};
    auto extOnTransaction = MockExtTransactionNftBurn{};
    auto extOnObject = MockExtOnObject{};

    auto transactions = std::vector{
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::CreateTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };
    auto objects = std::vector{
        util::CreateObject(),
        util::CreateObject(),
        util::CreateObject(),
    };

    // testing::Sequence seq;
    testing::InSequence const seqGuard;
    EXPECT_CALL(extLedgerData, onLedgerData);
    EXPECT_CALL(extOnTransaction, onTransaction).Times(2);
    EXPECT_CALL(extOnObject, onObject).Times(3);

    auto const header = CreateLedgerHeader(LedgerHash, Seq);
    auto reg = Registry<MockExtOnObject&, MockExtTransactionNftBurn&, MockExtLedgerData&>(
        extOnObject, extOnTransaction, extLedgerData
    );
    reg.dispatch(etlng::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = std::move(objects),
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = Seq
    });
}
