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

#include "data/AmendmentCenter.hpp"
#include "data/Types.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace data;

constexpr auto SEQ = 30u;

struct AmendmentCenterTest : util::prometheus::WithPrometheus, MockBackendTest, SyncAsioContextTest {
    AmendmentCenter amendmentCenter{backend};
};

// This is a safety net test that will fail anytime we built Clio against a new libXRPL that added some Amendment that
// we forgot to register in data::Amendments.
TEST_F(AmendmentCenterTest, AllAmendmentsFromLibXRPLAreSupported)
{
    for (auto const& [name, _] : ripple::allAmendments()) {
        ASSERT_TRUE(amendmentCenter.isSupported(name)) << "XRPL amendment not supported by Clio: " << name;
    }

    ASSERT_EQ(amendmentCenter.getSupported().size(), ripple::allAmendments().size());
    ASSERT_EQ(amendmentCenter.getAll().size(), ripple::allAmendments().size());
}

TEST_F(AmendmentCenterTest, Accessors)
{
    {
        auto const am = amendmentCenter.getAmendment("DisallowIncoming");
        EXPECT_EQ(am.feature, ripple::uint256("47C3002ABA31628447E8E9A8B315FAA935CE30183F9A9B86845E469CA2CDC3DF"));
    }
    {
        auto const am = amendmentCenter["DisallowIncoming"];
        EXPECT_EQ(am.feature, ripple::uint256("47C3002ABA31628447E8E9A8B315FAA935CE30183F9A9B86845E469CA2CDC3DF"));
    }

    auto const a = amendmentCenter[Amendments::OwnerPaysFee];
    auto const b = amendmentCenter["OwnerPaysFee"];
    EXPECT_EQ(a, b);
}

TEST_F(AmendmentCenterTest, IsEnabled)
{
    EXPECT_TRUE(amendmentCenter.isSupported("fixUniversalNumber"));
    EXPECT_FALSE(amendmentCenter.isSupported("unknown"));

    auto const amendments = CreateAmendmentsObject({Amendments::fixUniversalNumber});
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, SEQ, testing::_))
        .WillRepeatedly(testing::Return(amendments.getSerializer().peekData()));

    EXPECT_TRUE(amendmentCenter.isEnabled("fixUniversalNumber", SEQ));
    EXPECT_FALSE(amendmentCenter.isEnabled("unknown", SEQ));
    EXPECT_FALSE(amendmentCenter.isEnabled("ImmediateOfferKilled", SEQ));
}

TEST_F(AmendmentCenterTest, IsMultipleEnabled)
{
    auto const amendments = CreateAmendmentsObject({Amendments::fixUniversalNumber});
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, SEQ, testing::_))
        .WillOnce(testing::Return(amendments.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        std::vector<data::AmendmentKey> keys{"fixUniversalNumber", "unknown", "ImmediateOfferKilled"};
        auto const result = amendmentCenter.isEnabled(yield, keys, SEQ);

        EXPECT_EQ(result.size(), keys.size());
        EXPECT_TRUE(result.at(0));
        EXPECT_FALSE(result.at(1));
        EXPECT_FALSE(result.at(2));
    });
}

TEST_F(AmendmentCenterTest, IsEnabledThrowsWhenUnavailable)
{
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, SEQ, testing::_))
        .WillOnce(testing::Return(std::nullopt));

    runSpawn([this](auto yield) {
        EXPECT_THROW(
            { [[maybe_unused]] auto const result = amendmentCenter.isEnabled(yield, "irrelevant", SEQ); },
            std::runtime_error
        );
    });
}

TEST_F(AmendmentCenterTest, IsEnabledReturnsFalseWhenNoAmendments)
{
    auto const amendments = CreateBrokenAmendmentsObject();
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, SEQ, testing::_))
        .WillOnce(testing::Return(amendments.getSerializer().peekData()));

    runSpawn([this](auto yield) { EXPECT_FALSE(amendmentCenter.isEnabled(yield, "irrelevant", SEQ)); });
}

TEST_F(AmendmentCenterTest, IsEnabledReturnsVectorOfFalseWhenNoAmendments)
{
    auto const amendments = CreateBrokenAmendmentsObject();
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, SEQ, testing::_))
        .WillOnce(testing::Return(amendments.getSerializer().peekData()));

    runSpawn([this](auto yield) {
        std::vector<data::AmendmentKey> keys{"fixUniversalNumber", "ImmediateOfferKilled"};
        auto const vec = amendmentCenter.isEnabled(yield, keys, SEQ);

        EXPECT_EQ(vec.size(), keys.size());
        EXPECT_TRUE(std::ranges::all_of(vec, [](bool val) { return val == false; }));
    });
}

TEST(AmendmentTest, GenerateAmendmentId)
{
    // https://xrpl.org/known-amendments.html#disallowincoming refer to the published id
    EXPECT_EQ(
        ripple::uint256("47C3002ABA31628447E8E9A8B315FAA935CE30183F9A9B86845E469CA2CDC3DF"),
        Amendment::GetAmendmentId("DisallowIncoming")
    );
}

struct AmendmentCenterDeathTest : AmendmentCenterTest {};

TEST_F(AmendmentCenterDeathTest, GetInvalidAmendmentAsserts)
{
    EXPECT_DEATH({ [[maybe_unused]] auto _ = amendmentCenter.getAmendment("invalidAmendmentKey"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto _ = amendmentCenter["invalidAmendmentKey"]; }, ".*");
}

struct AmendmentKeyTest : testing::Test {};

TEST_F(AmendmentKeyTest, Convertible)
{
    std::string const key1 = "key1";
    auto key2 = "key2";

    EXPECT_NO_THROW({
        auto const first = AmendmentKey(key1);
        auto const second = AmendmentKey(key2);
        auto const third = AmendmentKey("test");

        std::string const s1 = first;
        EXPECT_EQ(s1, key1);

        ripple::uint256 const k1 = first;
        ripple::uint256 const k2 = second;

        EXPECT_EQ(k1, ripple::uint256{"7E365F775657DC0EB960E6295A1F44B3F67479F54D5D12C5D87E6DB234F072E3"});
        EXPECT_EQ(k2, ripple::uint256{"B4F33541E0E2FC2F7AA17D2D2E6A9B424809123485251D3413E91CC462309772"});
    });
}

TEST_F(AmendmentKeyTest, Comparison)
{
    auto const first = AmendmentKey("1");
    auto const second = AmendmentKey("2");
    EXPECT_GT(second, first);
}
