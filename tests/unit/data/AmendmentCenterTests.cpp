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
#include "util/Fixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>

#include <string>

using namespace data;

constexpr auto SEQ = 30;

struct AmendmentCenterTest : util::prometheus::WithPrometheus, MockBackendTest {
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
    EXPECT_DEATH({ amendmentCenter.getAmendment("invalidAmendmentKey"); }, ".*");
    EXPECT_DEATH({ amendmentCenter["invalidAmendmentKey"]; }, ".*");
}

struct AmendmentKeyTest : testing::Test {};

TEST_F(AmendmentKeyTest, Convertible)
{
    std::string key1 = "key1";
    auto key2 = "key2";

    EXPECT_NO_THROW({
        auto const first = AmendmentKey(key1);
        auto const second = AmendmentKey(key2);
        auto const third = AmendmentKey("test");

        std::string s1 = first;
        EXPECT_EQ(s1, key1);

        ripple::uint256 k1 = first;
        ripple::uint256 k2 = second;

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
