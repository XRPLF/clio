//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
#include "etlng/impl/ext/Cache.hpp"
#include "util/BinaryTestObject.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

using namespace etlng::impl;
using namespace data;

namespace {
constinit auto const kSEQ = 123u;
constinit auto const kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constinit auto const kUNUSED_LAST_KEY = "unused";

auto
createTestData()
{
    auto objects = std::vector{util::createObject(), util::createObject(), util::createObject()};
    auto const header = createLedgerHeader(kLEDGER_HASH, kSEQ);
    return etlng::model::LedgerData{
        .transactions = {},
        .objects = std::move(objects),
        .successors = {},
        .edgeKeys = {},
        .header = header,
        .rawHeader = {},
        .seq = kSEQ
    };
}

}  // namespace

struct CacheExtTests : util::prometheus::WithPrometheus {
protected:
    MockLedgerCache cache_;
    etlng::impl::CacheExt ext_{cache_};
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

    EXPECT_CALL(cache_, update(objects, kSEQ));

    ext_.onInitialObjects(kSEQ, objects, kUNUSED_LAST_KEY);
}
