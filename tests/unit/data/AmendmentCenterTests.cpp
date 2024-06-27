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
