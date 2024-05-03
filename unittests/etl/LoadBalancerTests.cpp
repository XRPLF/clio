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
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gtest/gtest.h>

#include <memory>
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

    std::unique_ptr<LoadBalancer>
    makeLoadBalancer()
    {
        return std::make_unique<LoadBalancer>(
            util::Config{configJson_},
            ioContext_,
            backend,
            subscriptionManager_,
            networkManager_,
            [this](auto&&... args) -> SourcePtr {
                return sourceFactory_.makeSourceMock(std::forward<decltype(args)>(args)...);
            }
        );
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

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSource0Failed)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(std::nullopt));
    EXPECT_CALL(sourceFactory_.sourceAt(0), toString);
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSource0ReturnedError)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{{"error", "some error"}}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), toString);
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSource1Failed)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(std::nullopt));
    EXPECT_CALL(sourceFactory_.sourceAt(1), toString);
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSourceDifferentNetworkID)
{
    auto const source1Json = boost::json::parse(R"({"result": {"info": {"network_id": 0}}})");
    auto const source2Json = boost::json::parse(R"({"result": {"info": {"network_id": 1}}})");

    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(source1Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(source2Json.as_object()));
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSourceFailedButAllowNoEtlIsTrue)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(std::nullopt));
    EXPECT_CALL(sourceFactory_.sourceAt(1), toString);
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);

    configJson_.as_object()["allow_no_etl"] = true;
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLStateFromSourceDifferentNetworkIDButAllowNoEtlIsTrue)
{
    auto const source1Json = boost::json::parse(R"({"result": {"info": {"network_id": 0}}})");
    auto const source2Json = boost::json::parse(R"({"result": {"info": {"network_id": 1}}})");
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(source1Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(source2Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);

    configJson_.as_object()["allow_no_etl"] = true;
    makeLoadBalancer();
}

struct LoadBalancerOnConnectHookTests : LoadBalancerConstructorTests {
    LoadBalancerOnConnectHookTests()
    {
        EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(0), run);
        EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(1), run);
        loadBalancer_ = makeLoadBalancer();
    }
    std::unique_ptr<LoadBalancer> loadBalancer_;
};

TEST_F(LoadBalancerOnConnectHookTests, sourcesConnect)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(0).onConnect();
    sourceFactory_.callbacksAt(1).onConnect();
}

TEST_F(LoadBalancerOnConnectHookTests, sourcesConnect_Source0IsNotConnected)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(0).onConnect();  // assuming it connects and disconnects immediately

    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(true));
    sourceFactory_.callbacksAt(1).onConnect();

    // Nothing is called on another connect
    sourceFactory_.callbacksAt(0).onConnect();
}

TEST_F(LoadBalancerOnConnectHookTests, sourcesConnect_BothSourcesAreNotConnected)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(0).onConnect();

    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(1).onConnect();

    // Then source 0 got connected
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(0).onConnect();
}

struct LoadBalancerOnDisconnectHookTests : LoadBalancerOnConnectHookTests {
    LoadBalancerOnDisconnectHookTests()
    {
        EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(true));
        EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(true));
        EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
        sourceFactory_.callbacksAt(0).onConnect();

        // nothing happens on source 1 connect
        sourceFactory_.callbacksAt(1).onConnect();
    }
};

TEST_F(LoadBalancerOnDisconnectHookTests, source0Disconnected)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(true));
    sourceFactory_.callbacksAt(0).onDisconnect();
}

TEST_F(LoadBalancerOnDisconnectHookTests, source1Disconnected)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(1).onDisconnect();
}

TEST_F(LoadBalancerOnDisconnectHookTests, source0DisconnectedAndConnectedBack)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(true));
    sourceFactory_.callbacksAt(0).onDisconnect();

    sourceFactory_.callbacksAt(0).onConnect();
}

TEST_F(LoadBalancerOnDisconnectHookTests, source1DisconnectedAndConnectedBack)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(1).onDisconnect();

    sourceFactory_.callbacksAt(1).onConnect();
}

TEST_F(LoadBalancerOnConnectHookTests, bothSourcesDisconnectsAndConnectsBack)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).Times(2).WillRepeatedly(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false)).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).Times(2).WillRepeatedly(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false)).Times(2);
    sourceFactory_.callbacksAt(0).onDisconnect();
    sourceFactory_.callbacksAt(1).onDisconnect();

    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(0).onConnect();

    sourceFactory_.callbacksAt(1).onConnect();
}

struct LoadBalancer3SourcesTests : LoadBalancerConstructorTests {
    LoadBalancer3SourcesTests()
    {
        sourceFactory_ = StrictMockSourceFactory{3};
        configJson_.as_object()["etl_sources"] = {"source1", "source2", "source3"};
        EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(0), run);
        EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(1), run);
        EXPECT_CALL(sourceFactory_.sourceAt(2), forwardToRippled).WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(2), run);
        loadBalancer_ = makeLoadBalancer();
    }
    std::unique_ptr<LoadBalancer> loadBalancer_;
};

TEST_F(LoadBalancer3SourcesTests, ForwardingUpdate)
{
    // Source 2 is connected first
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(2), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(2), setForwarding(true));
    sourceFactory_.callbacksAt(2).onConnect();

    // Then source 0 and 1 are getting connected, but nothing should happen
    sourceFactory_.callbacksAt(0).onConnect();
    sourceFactory_.callbacksAt(1).onConnect();

    // Source 0 got disconnected
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(true));
    EXPECT_CALL(sourceFactory_.sourceAt(2), setForwarding(false));  // only source 1 must be forwarding
    sourceFactory_.callbacksAt(0).onDisconnect();
}

struct LoadBalancerLoadInitialLedgerTests : LoadBalancerOnDisconnectHookTests {};

TEST_F(LoadBalancerLoadInitialLedgerTests, loadInitialLedger)
{
}
// loadInitialLedger
// download ranges (num_markers)

// fetchLedger

// forwardToRippled
// forwarding cache
// onledgerClosed hook called

// toJson
// getETLState
