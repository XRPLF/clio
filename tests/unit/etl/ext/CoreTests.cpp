#include "etl/Models.hpp"
#include "etl/impl/ext/Core.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/TxFormats.h>

#include <utility>
#include <vector>

using namespace etl::impl;
using namespace data;

namespace {
constinit auto const kSEQ = 123u;
constinit auto const kLEDGER_HASH =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

auto
createTestData()
{
    auto transactions = std::vector{
        util::createTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::createTransaction(ripple::TxType::ttNFTOKEN_BURN),
        util::createTransaction(ripple::TxType::ttNFTOKEN_CREATE_OFFER),
    };

    auto const header = createLedgerHeader(kLEDGER_HASH, kSEQ);
    return etl::model::LedgerData{
        .transactions = std::move(transactions),
        .objects = {},
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSEQ
    };
}

}  // namespace

struct CoreExtTests : util::prometheus::WithPrometheus, MockBackendTest {
protected:
    etl::impl::CoreExt ext_{backend_};
};

TEST_F(CoreExtTests, OnLedgerDataWritesLedgerAndTransactions)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, writeLedger(testing::_, auto{data.rawHeader}));
    EXPECT_CALL(*backend_, writeAccountTransaction).Times(data.transactions.size());
    EXPECT_CALL(*backend_, writeTransaction).Times(data.transactions.size());

    ext_.onLedgerData(data);
}

TEST_F(CoreExtTests, OnInitialDataWritesLedgerAndTransactions)
{
    auto const data = createTestData();

    EXPECT_CALL(*backend_, writeLedger(testing::_, auto{data.rawHeader}));
    EXPECT_CALL(*backend_, writeAccountTransaction).Times(data.transactions.size());
    EXPECT_CALL(*backend_, writeTransaction).Times(data.transactions.size());

    ext_.onInitialData(data);
}

TEST_F(CoreExtTests, OnInitialObjectWritesLedgerObject)
{
    auto const data = util::createObject();

    EXPECT_CALL(*backend_, writeLedgerObject(auto{data.keyRaw}, kSEQ, auto{data.dataRaw}));

    ext_.onInitialObject(kSEQ, data);
}

TEST_F(CoreExtTests, OnObjectWritesLedgerObject)
{
    auto const data = util::createObject();

    EXPECT_CALL(*backend_, writeLedgerObject(auto{data.keyRaw}, kSEQ, auto{data.dataRaw}));

    ext_.onObject(kSEQ, data);
}
