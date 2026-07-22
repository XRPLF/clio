#include "etl/Models.hpp"
#include "etl/impl/CacheUpdater.hpp"
#include "etl/impl/ext/Cache.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

using namespace etl::impl;
using namespace data;

namespace {
constinit auto const kSeq = 123u;
constinit auto const kLedgerHash =
    "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kUnusedLastKey = "unused";

auto
createTestData()
{
    auto objects = std::vector{util::createObject(), util::createObject(), util::createObject()};
    auto const header = createLedgerHeader(kLedgerHash, kSeq);
    return etl::model::LedgerData{
        .transactions = {},
        .objects = std::move(objects),
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSeq
    };
}

}  // namespace

struct CacheExtTests : util::prometheus::WithPrometheus {
protected:
    MockLedgerCache cache_;
    std::shared_ptr<etl::impl::CacheUpdater> updater_ =
        std::make_shared<etl::impl::CacheUpdater>(cache_);
    etl::impl::CacheExt ext_{updater_};
};

TEST_F(CacheExtTests, OnLedgerDataUpdatesCache)
{
    auto const data = createTestData();

    EXPECT_CALL(cache_, update(data.objects, data.seq));

    ext_.onLedgerData(data);
}

TEST_F(CacheExtTests, OnInitialDataUpdatesCacheAndSetsFull)
{
    auto const data = createTestData();

    EXPECT_CALL(cache_, update(data.objects, data.seq));
    EXPECT_CALL(cache_, setFull);

    ext_.onInitialData(data);
}

TEST_F(CacheExtTests, OnInitialObjectsUpdateCache)
{
    auto const objects = std::vector{util::createObject(), util::createObject()};

    EXPECT_CALL(cache_, update(objects, kSeq));

    ext_.onInitialObjects(kSeq, objects, kUnusedLastKey);
}

TEST_F(CacheExtTests, AllowInReadonlyReturnsTrue)
{
    EXPECT_TRUE(ext_.allowInReadonly());
}
