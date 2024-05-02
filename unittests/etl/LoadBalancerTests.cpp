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

#include "etl/LoadBalancer.hpp"
#include "etl/Source.hpp"
#include "gmock/gmock.h"
#include "util/Fixtures.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSource.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/config/Config.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>

using namespace etl;
using testing::Return;

struct LoadBalancerConstructorTests : util::prometheus::WithPrometheus, MockBackendTestStrict {
    StrictMockSubscriptionManagerSharedPtr subscriptionManager_;
    StrictMockNetworkValidatedLedgersPtr networkManager_;
    StrictMockSourceFactory sourceFactory_{2};
    boost::asio::io_context ioContext_;
    boost::json::value configJson_{{"etl_sources", {"source1", "source2"}}};

    LoadBalancer
    makeLoadBalancer()
    {
        return LoadBalancer{
            util::Config{configJson_},
            ioContext_,
            backend,
            subscriptionManager_,
            networkManager_,
            [this](auto&&... args) -> SourcePtr {
                return sourceFactory_.makeSourceMock(std::forward<decltype(args)>(args)...);
            }
        };
    }
};

TEST_F(LoadBalancerConstructorTests, Construct)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSource1Failed)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(std::nullopt));
    EXPECT_CALL(sourceFactory_.sourceAt(0), toString);
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSource1ReturnedError)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{{"error", "some error"}}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), toString);
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSource2Failed)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(std::nullopt));
    EXPECT_CALL(sourceFactory_.sourceAt(1), toString);
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSourceDifferentNetworkID)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{{"result", {"info", {"network_id", 1}}}}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(boost::json::object{{"result", {"info", {"network_id", 2}}}}));
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

// fetchETLStateFromSource failed for source 1
// fetchETLStateFromSource failed for source 2
// fetchETLStateFromSource different networkID
// fetchETLStateFromSource failed but allowNoEtl is true
// fetchETLStateFromSource different networkID but allowNoEtl is true

// onConnect hook called
// onDisconnect hook called
// onledgerClosed hook called

// loadInitialLesger
// download ranges (num_markers)

// fetchLedger

// forwardToRippled
// forwarding cache

// toJson
// getETLState
