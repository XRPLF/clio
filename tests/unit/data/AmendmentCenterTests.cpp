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
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>

using namespace testing;
using namespace data;

constexpr auto SEQ = 30;

struct AmendmentCenterTest : util::prometheus::WithPrometheus, MockBackendTest {};

TEST_F(AmendmentCenterTest, Amendments)
{
    auto amendmentCenter = AmendmentCenter{backend};
    EXPECT_TRUE(amendmentCenter.isSupported("fixUniversalNumber"));
    EXPECT_FALSE(amendmentCenter.isSupported("unknown"));

    EXPECT_EQ(amendmentCenter.getAll().size(), ripple::detail::supportedAmendments().size());

    auto const amendments = CreateAmendmentsObject({Amendments::fixUniversalNumber});
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, SEQ, _))
        .WillRepeatedly(testing::Return(amendments.getSerializer().peekData()));

    EXPECT_TRUE(amendmentCenter.isEnabled("fixUniversalNumber", SEQ));
    EXPECT_FALSE(amendmentCenter.isEnabled("unknown", SEQ));
    EXPECT_FALSE(amendmentCenter.isEnabled("ImmediateOfferKilled", SEQ));
}

TEST_F(AmendmentCenterTest, GenerateAmendmentId)
{
    // https://xrpl.org/known-amendments.html#disallowincoming refer to the published id
    EXPECT_EQ(
        ripple::uint256("47C3002ABA31628447E8E9A8B315FAA935CE30183F9A9B86845E469CA2CDC3DF"),
        Amendment::GetAmendmentId("DisallowIncoming")
    );
}
