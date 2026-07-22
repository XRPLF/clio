#include "etl/Models.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/Registry.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <boost/signals2/connection.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/TxFormats.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace etl::impl;

namespace compiletime::checks {

struct Ext1 {
    static void
    onLedgerData(etl::model::LedgerData const&);
};

struct Ext2 {
    static void
    onInitialObjects(uint32_t, std::vector<etl::model::Object> const&, std::string);
};

struct Ext3 {
    static void
    onInitialData(etl::model::LedgerData const&);
};

struct Ext4SpecMissing {
    static void
    onTransaction(uint32_t, etl::model::Transaction const&);
};

struct Ext4Fixed {
    using spec = etl::model::Spec<xrpl::TxType::ttNFTOKEN_BURN>;

    static void
    onTransaction(uint32_t, etl::model::Transaction const&);
};

struct Ext5 {
    static void
    onInitialObject(uint32_t, etl::model::Object const&);
};

struct Ext6SpecMissing {
    static void
    onInitialTransaction(uint32_t, etl::model::Transaction const&);
};

struct Ext6Fixed {
    using spec = etl::model::Spec<xrpl::TxType::ttNFTOKEN_BURN>;

    static void
    onInitialTransaction(uint32_t, etl::model::Transaction const&);
};

struct ExtRealistic {
    using spec = etl::model::Spec<
        xrpl::TxType::ttNFTOKEN_BURN,
        xrpl::TxType::ttNFTOKEN_ACCEPT_OFFER,
        xrpl::TxType::ttNFTOKEN_CREATE_OFFER,
        xrpl::TxType::ttNFTOKEN_CANCEL_OFFER,
        xrpl::TxType::ttNFTOKEN_MINT>;

    static void
    onLedgerData(etl::model::LedgerData const&);
    static void
    onInitialObject(uint32_t, etl::model::Object const&);
    static void
    onInitialTransaction(uint32_t, etl::model::Transaction const&);
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
    using spec = etl::model::Spec<xrpl::ttNFTOKEN_BURN, xrpl::ttNFTOKEN_MINT>;
};

// invalid spec does not compile:
// struct DuplicatesSpec {
//     using spec = etl::model::Spec<xrpl::ttNFTOKEN_BURN, xrpl::ttNFTOKEN_BURN,
//     xrpl::ttNFTOKEN_MINT>;
// };

static_assert(ContainsSpec<ValidSpec>);

}  // namespace compiletime::checks

namespace {

constinit auto const kLedgerHash =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kSeq = 30;

struct MockExtLedgerData {
    MOCK_METHOD(void, onLedgerData, (etl::model::LedgerData const&), (const));
};

struct MockExtInitialData {
    MOCK_METHOD(void, onInitialData, (etl::model::LedgerData const&), (const));
};

struct MockExtOnObject {
    MOCK_METHOD(void, onObject, (uint32_t, etl::model::Object const&), (const));
};

struct MockExtTransactionNftBurn {
    using spec = etl::model::Spec<xrpl::TxType::ttNFTOKEN_BURN>;
    MOCK_METHOD(void, onTransaction, (uint32_t, etl::model::Transaction const&), (const));
};

struct MockExtTransactionNftOffer {
    using spec = etl::model::Spec<
        xrpl::TxType::ttNFTOKEN_CREATE_OFFER,
        xrpl::TxType::ttNFTOKEN_CANCEL_OFFER,
        xrpl::TxType::ttNFTOKEN_ACCEPT_OFFER>;
    MOCK_METHOD(void, onTransaction, (uint32_t, etl::model::Transaction const&), (const));
};

struct MockExtInitialObject {
    MOCK_METHOD(void, onInitialObject, (uint32_t, etl::model::Object const&), (const));
};

struct MockExtInitialObjects {
    MOCK_METHOD(
        void,
        onInitialObjects,
        (uint32_t, std::vector<etl::model::Object> const&, std::string),
        (const)
    );
};

struct MockExtNftBurn {
    using spec = etl::model::Spec<xrpl::TxType::ttNFTOKEN_BURN>;
    MOCK_METHOD(void, onInitialTransaction, (uint32_t, etl::model::Transaction const&), (const));
};

struct MockExtNftOffer {
    using spec = etl::model::Spec<
        xrpl::TxType::ttNFTOKEN_CREATE_OFFER,
        xrpl::TxType::ttNFTOKEN_CANCEL_OFFER,
        xrpl::TxType::ttNFTOKEN_ACCEPT_OFFER>;
    MOCK_METHOD(void, onInitialTransaction, (uint32_t, etl::model::Transaction const&), (const));
};

// Mock extensions with allowInReadonly
struct MockExtLedgerDataReadonly {
    MOCK_METHOD(void, onLedgerData, (etl::model::LedgerData const&), (const));

    static bool
    allowInReadonly()
    {
        return true;
    }
};

struct MockExtInitialDataReadonly {
    MOCK_METHOD(void, onInitialData, (etl::model::LedgerData const&), (const));

    static bool
    allowInReadonly()
    {
        return true;
    }
};

struct MockExtOnObjectReadonly {
    MOCK_METHOD(void, onObject, (uint32_t, etl::model::Object const&), (const));

    static bool
    allowInReadonly()
    {
        return true;
    }
};

struct MockExtTransactionNftBurnReadonly {
    using spec = etl::model::Spec<xrpl::TxType::ttNFTOKEN_BURN>;
    MOCK_METHOD(void, onTransaction, (uint32_t, etl::model::Transaction const&), (const));

    static bool
    allowInReadonly()
    {
        return true;
    }
};

struct MockExtInitialObjectReadonly {
    MOCK_METHOD(void, onInitialObject, (uint32_t, etl::model::Object const&), (const));

    static bool
    allowInReadonly()
    {
        return true;
    }
};

struct MockExtInitialObjectsReadonly {
    MOCK_METHOD(
        void,
        onInitialObjects,
        (uint32_t, std::vector<etl::model::Object> const&, std::string),
        (const)
    );

    static bool
    allowInReadonly()
    {
        return true;
    }
};

struct MockExtNftBurnReadonly {
    using spec = etl::model::Spec<xrpl::TxType::ttNFTOKEN_BURN>;
    MOCK_METHOD(void, onInitialTransaction, (uint32_t, etl::model::Transaction const&), (const));

    static bool
    allowInReadonly()
    {
        return true;
    }
};

struct RegistryTest : util::prometheus::WithPrometheus {
    RegistryTest()
    {
        state_.isWriting = true;
    }

protected:
    etl::SystemState state_;
};

}  // namespace

TEST_F(RegistryTest, FilteringOfTxWorksCorrectlyForInitialTransaction)
{
    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto extBurn = MockExtNftBurn{};
    auto extOffer = MockExtNftOffer{};

    EXPECT_CALL(extBurn, onInitialTransaction(testing::_, testing::_)).Times(2);  // 2 burn txs
    EXPECT_CALL(extOffer, onInitialTransaction(testing::_, testing::_));          // 1 create offer

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtNftBurn&, MockExtNftOffer&>(state_, extBurn, extOffer);
    reg.dispatchInitialData(
        etl::model::LedgerData{
            .transactions = transactions,
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq,
        }
    );
}

TEST_F(RegistryTest, FilteringOfTxWorksCorrectlyForTransaction)
{
    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto extBurn = MockExtTransactionNftBurn{};
    auto extOffer = MockExtTransactionNftOffer{};

    EXPECT_CALL(extBurn, onTransaction(testing::_, testing::_)).Times(2);  // 2 burn txs
    EXPECT_CALL(extOffer, onTransaction(testing::_, testing::_));          // 1 create offer

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtTransactionNftBurn&, MockExtTransactionNftOffer&>(
        state_, extBurn, extOffer
    );
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, InitialObjectsEmpty)
{
    auto extObj = MockExtInitialObject{};
    auto extObjs = MockExtInitialObjects{};

    EXPECT_CALL(extObj, onInitialObject(testing::_, testing::_)).Times(0);  // 0 empty objects sent
    EXPECT_CALL(
        extObjs, onInitialObjects(testing::_, testing::_, testing::_)
    );  // 1 vector passed as is

    auto reg = Registry<MockExtInitialObject&, MockExtInitialObjects&>(state_, extObj, extObjs);
    reg.dispatchInitialObjects(kSeq, {}, {});
}

TEST_F(RegistryTest, InitialObjectsDispatched)
{
    auto extObj = MockExtInitialObject{};
    auto extObjs = MockExtInitialObjects{};

    EXPECT_CALL(extObj, onInitialObject(testing::_, testing::_)).Times(3);  // 3 objects sent
    EXPECT_CALL(
        extObjs, onInitialObjects(testing::_, testing::_, testing::_)
    );  // 1 vector passed as is

    auto reg = Registry<MockExtInitialObject&, MockExtInitialObjects&>(state_, extObj, extObjs);
    reg.dispatchInitialObjects(
        kSeq, {util::createObject(), util::createObject(), util::createObject()}, {}
    );
}

TEST_F(RegistryTest, ObjectsDispatched)
{
    auto extObj = MockExtOnObject{};

    EXPECT_CALL(extObj, onObject(testing::_, testing::_)).Times(3);  // 3 objects sent

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtOnObject&>(state_, extObj);
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = {},
            .objects = {util::createObject(), util::createObject(), util::createObject()},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, OnLedgerDataForBatch)
{
    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto ext = MockExtLedgerData{};

    EXPECT_CALL(ext, onLedgerData(testing::_));  // 1 batch (dispatch call)

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtLedgerData&>(state_, ext);
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, InitialObjectsCorrectOrderOfHookCalls)
{
    auto extObjs = MockExtInitialObjects{};
    auto extObj = MockExtInitialObject{};

    testing::InSequence const seqGuard;
    EXPECT_CALL(extObjs, onInitialObjects);
    EXPECT_CALL(extObj, onInitialObject).Times(3);

    auto reg = Registry<MockExtInitialObject&, MockExtInitialObjects&>(state_, extObj, extObjs);
    reg.dispatchInitialObjects(
        kSeq, {util::createObject(), util::createObject(), util::createObject()}, {}
    );
}

TEST_F(RegistryTest, InitialDataCorrectOrderOfHookCalls)
{
    auto extInitialData = MockExtInitialData{};
    auto extInitialTransaction = MockExtNftBurn{};

    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    testing::InSequence const seqGuard;
    EXPECT_CALL(extInitialData, onInitialData);
    EXPECT_CALL(extInitialTransaction, onInitialTransaction).Times(2);

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtNftBurn&, MockExtInitialData&>(
        state_, extInitialTransaction, extInitialData
    );
    reg.dispatchInitialData(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, LedgerDataCorrectOrderOfHookCalls)
{
    auto extLedgerData = MockExtLedgerData{};
    auto extOnTransaction = MockExtTransactionNftBurn{};
    auto extOnObject = MockExtOnObject{};

    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_CREATE_OFFER),
    };
    auto objects = std::vector{
        util::createObject(),
        util::createObject(),
        util::createObject(),
    };

    // testing::Sequence seq;
    testing::InSequence const seqGuard;
    EXPECT_CALL(extLedgerData, onLedgerData);
    EXPECT_CALL(extOnTransaction, onTransaction).Times(2);
    EXPECT_CALL(extOnObject, onObject).Times(3);

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtOnObject&, MockExtTransactionNftBurn&, MockExtLedgerData&>(
        state_, extOnObject, extOnTransaction, extLedgerData
    );
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = std::move(objects),
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, ReadonlyModeLedgerDataAllowed)
{
    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
    };

    auto ext = MockExtLedgerDataReadonly{};
    state_.isWriting = false;

    EXPECT_CALL(ext, onLedgerData(testing::_));

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtLedgerDataReadonly&>(state_, ext);
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, ReadonlyModeTransactionAllowed)
{
    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
    };

    auto extTx = MockExtTransactionNftBurnReadonly{};
    state_.isWriting = false;

    EXPECT_CALL(extTx, onTransaction(testing::_, testing::_)).Times(2);

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtTransactionNftBurnReadonly&>(state_, extTx);
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, ReadonlyModeObjectAllowed)
{
    auto objects = std::vector{
        util::createObject(),
        util::createObject(),
        util::createObject(),
    };

    auto extObj = MockExtOnObjectReadonly{};
    state_.isWriting = false;

    EXPECT_CALL(extObj, onObject(testing::_, testing::_)).Times(3);

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtOnObjectReadonly&>(state_, extObj);
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = {},
            .objects = std::move(objects),
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, ReadonlyModeInitialDataAllowed)
{
    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
    };

    auto extInitialData = MockExtInitialDataReadonly{};
    state_.isWriting = false;

    EXPECT_CALL(extInitialData, onInitialData(testing::_));

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtInitialDataReadonly&>(state_, extInitialData);
    reg.dispatchInitialData(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, ReadonlyModeInitialTransactionAllowed)
{
    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
    };

    auto extTx = MockExtNftBurnReadonly{};
    state_.isWriting = false;

    EXPECT_CALL(extTx, onInitialTransaction(testing::_, testing::_)).Times(2);

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtNftBurnReadonly&>(state_, extTx);
    reg.dispatchInitialData(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, ReadonlyModeInitialObjectAllowed)
{
    auto extObj = MockExtInitialObjectReadonly{};
    state_.isWriting = false;

    EXPECT_CALL(extObj, onInitialObject(testing::_, testing::_)).Times(3);

    auto reg = Registry<MockExtInitialObjectReadonly&>(state_, extObj);
    reg.dispatchInitialObjects(
        kSeq, {util::createObject(), util::createObject(), util::createObject()}, {}
    );
}

TEST_F(RegistryTest, ReadonlyModeInitialObjectsAllowed)
{
    auto extObjs = MockExtInitialObjectsReadonly{};
    state_.isWriting = false;

    EXPECT_CALL(extObjs, onInitialObjects(testing::_, testing::_, testing::_));

    auto reg = Registry<MockExtInitialObjectsReadonly&>(state_, extObjs);
    reg.dispatchInitialObjects(
        kSeq, {util::createObject(), util::createObject(), util::createObject()}, {}
    );
}

TEST_F(RegistryTest, ReadonlyModeRegularExtensionsNotCalled)
{
    auto extLedgerData = MockExtLedgerData{};  // No allowInReadonly method
    auto objects = std::vector{
        util::createObject(),
        util::createObject(),
        util::createObject(),
    };

    state_.isWriting = false;

    EXPECT_CALL(extLedgerData, onLedgerData(testing::_))
        .Times(0);  // Should NOT be called in readonly mode

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<MockExtLedgerData&>(state_, extLedgerData);
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = {},
            .objects = std::move(objects),
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, MixedReadonlyAndRegularExtensions)
{
    auto extReadonly = MockExtLedgerDataReadonly{};
    auto extRegular = MockExtLedgerData{};
    auto objects = std::vector{
        util::createObject(),
        util::createObject(),
        util::createObject(),
    };

    state_.isWriting = false;

    EXPECT_CALL(extReadonly, onLedgerData(testing::_));
    EXPECT_CALL(extRegular, onLedgerData(testing::_))
        .Times(0);  // Should NOT be called in readonly mode

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg =
        Registry<MockExtLedgerDataReadonly&, MockExtLedgerData&>(state_, extReadonly, extRegular);
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = {},
            .objects = std::move(objects),
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, MonitorInterfaceExecution)
{
    struct MockMonitor : etl::MonitorInterface {
        MOCK_METHOD(void, notifySequenceLoaded, (uint32_t), (override));
        MOCK_METHOD(void, notifyWriteConflict, (uint32_t), (override));
        MOCK_METHOD(
            boost::signals2::scoped_connection,
            subscribeToNewSequence,
            (NewSequenceSignalType::slot_type const&),
            (override)
        );
        MOCK_METHOD(
            boost::signals2::scoped_connection,
            subscribeToDbStalled,
            (DbStalledSignalType::slot_type const&),
            (override)
        );
        MOCK_METHOD(void, run, (std::chrono::steady_clock::duration), (override));
        MOCK_METHOD(void, stop, (), (override));
    };

    auto monitor = MockMonitor{};
    EXPECT_CALL(monitor, notifySequenceLoaded(kSeq)).Times(1);

    monitor.notifySequenceLoaded(kSeq);
}

TEST_F(RegistryTest, ReadonlyModeWithAllowInReadonlyTest)
{
    struct ExtWithAllowInReadonly {
        MOCK_METHOD(void, onLedgerData, (etl::model::LedgerData const&), (const));

        static bool
        allowInReadonly()
        {
            return true;
        }
    };

    auto ext = ExtWithAllowInReadonly{};
    state_.isWriting = false;

    EXPECT_CALL(ext, onLedgerData(testing::_)).Times(1);

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<ExtWithAllowInReadonly&>(state_, ext);
    reg.dispatch(
        etl::model::LedgerData{
            .transactions = {},
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );
}

TEST_F(RegistryTest, ReadonlyModeExecutePluralHooksIfAllowedPaths)
{
    struct ExtWithBothHooksAndAllowReadonly {
        MOCK_METHOD(void, onLedgerData, (etl::model::LedgerData const&), (const));
        MOCK_METHOD(void, onInitialData, (etl::model::LedgerData const&), (const));
        MOCK_METHOD(
            void,
            onInitialObjects,
            (uint32_t, std::vector<etl::model::Object> const&, std::string),
            (const)
        );

        static bool
        allowInReadonly()
        {
            return true;
        }
    };

    auto ext = ExtWithBothHooksAndAllowReadonly{};
    state_.isWriting = false;

    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
    };
    auto objects = std::vector{
        util::createObject(),
    };

    EXPECT_CALL(ext, onLedgerData(testing::_)).Times(1);
    EXPECT_CALL(ext, onInitialData(testing::_)).Times(1);
    EXPECT_CALL(ext, onInitialObjects(testing::_, testing::_, testing::_)).Times(1);

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<ExtWithBothHooksAndAllowReadonly&>(state_, ext);

    reg.dispatch(
        etl::model::LedgerData{
            .transactions = transactions,
            .objects = objects,
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );

    reg.dispatchInitialData(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );

    reg.dispatchInitialObjects(kSeq, objects, {});
}

TEST_F(RegistryTest, ReadonlyModeExecuteByOneHooksIfAllowedPaths)
{
    struct ExtWithBothHooksAndAllowReadonly {
        using spec = etl::model::Spec<xrpl::TxType::ttNFTOKEN_BURN>;

        MOCK_METHOD(void, onObject, (uint32_t, etl::model::Object const&), (const));
        MOCK_METHOD(void, onInitialObject, (uint32_t, etl::model::Object const&), (const));
        MOCK_METHOD(void, onTransaction, (uint32_t, etl::model::Transaction const&), (const));
        MOCK_METHOD(
            void,
            onInitialTransaction,
            (uint32_t, etl::model::Transaction const&),
            (const)
        );

        static bool
        allowInReadonly()
        {
            return true;
        }
    };

    auto ext = ExtWithBothHooksAndAllowReadonly{};
    state_.isWriting = false;

    auto transactions = std::vector{
        util::createTransaction(xrpl::TxType::ttNFTOKEN_BURN),
    };
    auto objects = std::vector{
        util::createObject(),
    };

    EXPECT_CALL(ext, onTransaction(testing::_, testing::_)).Times(1);
    EXPECT_CALL(ext, onObject(testing::_, testing::_)).Times(1);
    EXPECT_CALL(ext, onInitialTransaction(testing::_, testing::_)).Times(1);
    EXPECT_CALL(ext, onInitialObject(testing::_, testing::_)).Times(1);

    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    auto reg = Registry<ExtWithBothHooksAndAllowReadonly&>(state_, ext);

    reg.dispatch(
        etl::model::LedgerData{
            .transactions = transactions,
            .objects = objects,
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );

    reg.dispatchInitialData(
        etl::model::LedgerData{
            .transactions = std::move(transactions),
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = header,
            .rawHeader = {},
            .seq = kSeq
        }
    );

    reg.dispatchInitialObjects(kSeq, objects, {});
}
