//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

/*
 * Use this file for temporary tests and implementations.
 * Note: Please don't push your temporary work to the repo.
 */

#include "data/AmendmentCenter.hpp"
#include "util/Fixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>

#include <vector>

using namespace testing;
using namespace data;

constexpr auto SEQ = 30;

struct PlaygroundTest : util::prometheus::WithPrometheus, MockBackendTest {};

TEST_F(PlaygroundTest, Amendments)
{
    auto man = AmendmentCenter{backend, xrplAmendments, {"fixUniversalNumber", "ImmediateOfferKilled"}};
    EXPECT_TRUE(man.isSupported("fixUniversalNumber"));
    EXPECT_FALSE(man.isSupported("unknown"));

    EXPECT_EQ(man.getAll().size(), ripple::detail::supportedAmendments().size());
    EXPECT_EQ(man.getSupported().size(), 2);

    auto const amendments = CreateAmendmentsObject({Amendment::GetAmendmentId("fixUniversalNumber")});
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, SEQ, _))
        .WillRepeatedly(testing::Return(amendments.getSerializer().peekData()));

    EXPECT_TRUE(man.isEnabled("fixUniversalNumber", SEQ));
    EXPECT_FALSE(man.isEnabled("unknown", SEQ));
    EXPECT_FALSE(man.isEnabled("ImmediateOfferKilled", SEQ));
}

TEST_F(PlaygroundTest, AmendmentsFoobar)
{
    auto mockAmendments = []() { return std::vector<Amendment>{Amendment("foo"), Amendment("bar")}; };
    auto man = AmendmentCenter{backend, mockAmendments, {"foo"}};

    EXPECT_EQ(man.getAll().size(), mockAmendments().size());
    EXPECT_EQ(man.getSupported().size(), 1);

    auto const amendments =
        CreateAmendmentsObject({Amendment::GetAmendmentId("foo"), Amendment::GetAmendmentId("bar")});
    EXPECT_CALL(*backend, doFetchLedgerObject(ripple::keylet::amendments().key, SEQ, _))
        .WillRepeatedly(testing::Return(amendments.getSerializer().peekData()));

    EXPECT_TRUE(man.isSupported("foo"));
    EXPECT_TRUE(man.isEnabled("foo", SEQ));
    EXPECT_FALSE(man.isEnabled("fixUniversalNumber1", SEQ));
    EXPECT_FALSE(man.isSupported("bar"));  // this can be used to check an amendment block too
    EXPECT_TRUE(man.isEnabled("bar", SEQ));
}
