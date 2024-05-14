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

#include "data/BackendInterface.hpp"
#include "rpc/Amendments.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/Fixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/digest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace testing;

struct Amendment {
    std::string name;
    ripple::uint256 feature;
    bool supportedByClio;

    Amendment(std::string n, bool supported = false)
        : name{std::move(n)}, feature{GetAmendmentId(name)}, supportedByClio{supported}
    {
    }

    static ripple::uint256
    GetAmendmentId(std::string_view const name)
    {
        return ripple::sha512Half(ripple::Slice(name.data(), name.size()));
    }
};

auto
xrplAmendments()
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    std::vector<Amendment> amendments;

    rg::copy(
        ripple::detail::supportedAmendments() | vs::transform([&](auto const& p) { return Amendment{p.first}; }),
        std::back_inserter(amendments)
    );

    return amendments;
}

auto
mockAmendments()
{
    return std::vector<Amendment>{Amendment("foo"), Amendment("bar")};
}

class AmendmentCenter {
    std::shared_ptr<data::BackendInterface> backend_;

    std::unordered_map<std::string, Amendment> supported_;
    std::vector<Amendment> all_;

public:
    AmendmentCenter(
        std::shared_ptr<data::BackendInterface> const& backend,
        auto provider,
        std::vector<std::string> amendments
    )
        : backend_{backend}
    {
        namespace rg = std::ranges;
        namespace vs = std::views;

        rg::copy(
            provider() | vs::transform([&](auto const& a) {
                auto const supported = rg::find(amendments, a.name) != rg::end(amendments);
                return Amendment{a.name, supported};
            }),
            std::back_inserter(all_)
        );

        for (auto const& am : all_ | vs::filter([](auto const& am) { return am.supportedByClio; }))
            supported_.insert_or_assign(am.name, am);
    }

    bool
    isSupported(std::string name) const
    {
        return supported_.contains(name) && supported_.at(name).supportedByClio;
    }

    std::unordered_map<std::string, Amendment> const&
    getSupported() const
    {
        return supported_;
    }

    std::vector<Amendment> const&
    getAll() const
    {
        return all_;
    }

    bool
    isEnabled(std::string name, uint32_t seq) const
    {
        namespace rg = std::ranges;

        if (auto am = rg::find(all_, name, [](auto const& am) { return am.name; }); am != rg::end(all_)) {
            return data::synchronous([this, feature = am->feature, seq](auto yield) {
                return rpc::isAmendmentEnabled(backend_, yield, seq, feature);
            });
        }

        return false;
    }
};

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
