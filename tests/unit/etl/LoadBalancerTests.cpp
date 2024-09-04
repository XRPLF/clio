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
#include "rpc/Errors.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSource.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/NameGenerator.hpp"
#include "util/Random.hpp"
#include "util/newconfig/ClioConfigFactories.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <grpcpp/support/status.h>
#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace etl;
using namespace util::config;
using testing::Return;

constexpr static auto const TwoSourcesLedgerResponse = R"({
    "etl_sources": [
        {
            "grpc_port": "source1"
        },
        {
            "grpc_port": "source2"
        }
    ]
})";

constexpr static auto const ThreeSourcesLedgerResponse = R"({
    "etl_sources": [
        {
            "grpc_port": "source1"
        },
        {
            "grpc_port": "source2"
        },
        {
            "grpc_port": "source3"
        }
    ]
})";

struct LoadBalancerConstructorTests : util::prometheus::WithPrometheus, MockBackendTestStrict {
    StrictMockSubscriptionManagerSharedPtr subscriptionManager_;
    StrictMockNetworkValidatedLedgersPtr networkManager_;
    StrictMockSourceFactory sourceFactory_{2};
    boost::asio::io_context ioContext_;
    boost::json::value configJson_{boost::json::parse(TwoSourcesLedgerResponse)};

    std::unique_ptr<LoadBalancer>
    makeLoadBalancer()
    {
        auto const cfg = getParseLoadBalancerConfig(configJson_);
        return std::make_unique<LoadBalancer>(
            cfg,
            ioContext_,
            backend,
            subscriptionManager_,
            networkManager_,
            [this](auto&&... args) -> SourcePtr { return sourceFactory_(std::forward<decltype(args)>(args)...); }
        );
    }
};

TEST_F(LoadBalancerConstructorTests, construct)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, forwardingTimeoutPassedToSourceFactory)
{
    auto const forwardingTimeout = 10;
    configJson_.as_object()["forwarding"] = boost::json::object{{"timeout", float{forwardingTimeout}}};
    EXPECT_CALL(
        sourceFactory_,
        makeSource(
            testing::_,
            testing::_,
            testing::_,
            testing::_,
            testing::_,
            std::chrono::steady_clock::duration{std::chrono::seconds{forwardingTimeout}},
            testing::_,
            testing::_,
            testing::_
        )
    )
        .Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_AllSourcesFail)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::etlCONNECTION_ERROR}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::etlCONNECTION_ERROR}));
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_AllSourcesReturnError)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{{"error", "some error"}}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(boost::json::object{{"error", "some error"}}));
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_Source1Fails0OK)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::etlCONNECTION_ERROR}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_Source0Fails1OK)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::etlCONNECTION_ERROR}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_DifferentNetworkID)
{
    auto const source1Json = boost::json::parse(R"({"result": {"info": {"network_id": 0}}})");
    auto const source2Json = boost::json::parse(R"({"result": {"info": {"network_id": 1}}})");

    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(source1Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(source2Json.as_object()));
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_AllSourcesFailButAllowNoEtlIsTrue)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::etlCONNECTION_ERROR}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);

    configJson_.as_object()["allow_no_etl"] = true;
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_DifferentNetworkIDButAllowNoEtlIsTrue)
{
    auto const source1Json = boost::json::parse(R"({"result": {"info": {"network_id": 0}}})");
    auto const source2Json = boost::json::parse(R"({"result": {"info": {"network_id": 1}}})");
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(source1Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(source2Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);

    configJson_.as_object()["allow_no_etl"] = true;
    makeLoadBalancer();
}

struct LoadBalancerConstructorDeathTest : LoadBalancerConstructorTests {};

TEST_F(LoadBalancerConstructorDeathTest, numMarkersSpecifiedInConfigIsInvalid)
{
    uint32_t const numMarkers = 257;
    configJson_.as_object()["num_markers"] = numMarkers;
    EXPECT_DEATH({ makeLoadBalancer(); }, ".*");
}

struct LoadBalancerOnConnectHookTests : LoadBalancerConstructorTests {
    LoadBalancerOnConnectHookTests()
    {
        EXPECT_CALL(sourceFactory_, makeSource).Times(2);
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

TEST_F(LoadBalancerOnDisconnectHookTests, source0Disconnects)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(true));
    sourceFactory_.callbacksAt(0).onDisconnect();
}

TEST_F(LoadBalancerOnDisconnectHookTests, source1Disconnects)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(1).onDisconnect();
}

TEST_F(LoadBalancerOnDisconnectHookTests, source0DisconnectsAndConnectsBack)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(true));
    sourceFactory_.callbacksAt(0).onDisconnect();

    sourceFactory_.callbacksAt(0).onConnect();
}

TEST_F(LoadBalancerOnDisconnectHookTests, source1DisconnectsAndConnectsBack)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(1).onDisconnect();

    sourceFactory_.callbacksAt(1).onConnect();
}

TEST_F(LoadBalancerOnConnectHookTests, bothSourcesDisconnectAndConnectBack)
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
        sourceFactory_.setSourcesNumber(3);
        configJson_ = boost::json::parse(ThreeSourcesLedgerResponse);

        EXPECT_CALL(sourceFactory_, makeSource).Times(3);
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

TEST_F(LoadBalancer3SourcesTests, forwardingUpdate)
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

struct LoadBalancerLoadInitialLedgerTests : LoadBalancerOnConnectHookTests {
    LoadBalancerLoadInitialLedgerTests()
    {
        util::Random::setSeed(0);
    }

    uint32_t const sequence_ = 123;
    uint32_t const numMarkers_ = 16;
    bool const cacheOnly_ = true;
    std::pair<std::vector<std::string>, bool> const response_ = {{"1", "2", "3"}, true};
};

TEST_F(LoadBalancerLoadInitialLedgerTests, load)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), loadInitialLedger(sequence_, numMarkers_, cacheOnly_))
        .WillOnce(Return(response_));

    EXPECT_EQ(loadBalancer_->loadInitialLedger(sequence_, cacheOnly_), response_.first);
}

TEST_F(LoadBalancerLoadInitialLedgerTests, load_source0DoesntHaveLedger)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), loadInitialLedger(sequence_, numMarkers_, cacheOnly_))
        .WillOnce(Return(response_));

    EXPECT_EQ(loadBalancer_->loadInitialLedger(sequence_, cacheOnly_), response_.first);
}

TEST_F(LoadBalancerLoadInitialLedgerTests, load_bothSourcesDontHaveLedger)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).Times(2).WillRepeatedly(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(false)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), loadInitialLedger(sequence_, numMarkers_, cacheOnly_))
        .WillOnce(Return(response_));

    EXPECT_EQ(loadBalancer_->loadInitialLedger(sequence_, cacheOnly_, std::chrono::milliseconds{1}), response_.first);
}

TEST_F(LoadBalancerLoadInitialLedgerTests, load_source0ReturnsStatusFalse)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), loadInitialLedger(sequence_, numMarkers_, cacheOnly_))
        .WillOnce(Return(std::make_pair(std::vector<std::string>{}, false)));
    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), loadInitialLedger(sequence_, numMarkers_, cacheOnly_))
        .WillOnce(Return(response_));

    EXPECT_EQ(loadBalancer_->loadInitialLedger(sequence_, cacheOnly_), response_.first);
}

struct LoadBalancerLoadInitialLedgerCustomNumMarkersTests : LoadBalancerConstructorTests {
    uint32_t const numMarkers_ = 16;
    uint32_t const sequence_ = 123;
    bool const cacheOnly_ = true;
    std::pair<std::vector<std::string>, bool> const response_ = {{"1", "2", "3"}, true};
};

TEST_F(LoadBalancerLoadInitialLedgerCustomNumMarkersTests, loadInitialLedger)
{
    configJson_.as_object()["num_markers"] = numMarkers_;

    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    auto loadBalancer = makeLoadBalancer();

    util::Random::setSeed(0);
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), loadInitialLedger(sequence_, numMarkers_, cacheOnly_))
        .WillOnce(Return(response_));

    EXPECT_EQ(loadBalancer->loadInitialLedger(sequence_, cacheOnly_), response_.first);
}

struct LoadBalancerFetchLegerTests : LoadBalancerOnConnectHookTests {
    LoadBalancerFetchLegerTests()
    {
        util::Random::setSeed(0);
        response_.second.set_validated(true);
    }
    uint32_t const sequence_ = 123;
    bool const getObjects_ = true;
    bool const getObjectNeighbors_ = false;
    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse> response_ =
        std::make_pair(grpc::Status::OK, org::xrpl::rpc::v1::GetLedgerResponse{});
};

TEST_F(LoadBalancerFetchLegerTests, fetch)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), fetchLedger(sequence_, getObjects_, getObjectNeighbors_))
        .WillOnce(Return(response_));

    EXPECT_TRUE(loadBalancer_->fetchLedger(sequence_, getObjects_, getObjectNeighbors_).has_value());
}

TEST_F(LoadBalancerFetchLegerTests, fetch_Source0ReturnsBadStatus)
{
    auto source0Response = response_;
    source0Response.first = grpc::Status::CANCELLED;

    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), fetchLedger(sequence_, getObjects_, getObjectNeighbors_))
        .WillOnce(Return(source0Response));

    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), fetchLedger(sequence_, getObjects_, getObjectNeighbors_))
        .WillOnce(Return(response_));

    EXPECT_TRUE(loadBalancer_->fetchLedger(sequence_, getObjects_, getObjectNeighbors_).has_value());
}

TEST_F(LoadBalancerFetchLegerTests, fetch_Source0ReturnsNotValidated)
{
    auto source0Response = response_;
    source0Response.second.set_validated(false);

    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), fetchLedger(sequence_, getObjects_, getObjectNeighbors_))
        .WillOnce(Return(source0Response));

    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), fetchLedger(sequence_, getObjects_, getObjectNeighbors_))
        .WillOnce(Return(response_));

    EXPECT_TRUE(loadBalancer_->fetchLedger(sequence_, getObjects_, getObjectNeighbors_).has_value());
}

TEST_F(LoadBalancerFetchLegerTests, fetch_bothSourcesFail)
{
    auto badResponse = response_;
    badResponse.second.set_validated(false);

    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), fetchLedger(sequence_, getObjects_, getObjectNeighbors_))
        .WillOnce(Return(badResponse))
        .WillOnce(Return(response_));

    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), fetchLedger(sequence_, getObjects_, getObjectNeighbors_))
        .WillOnce(Return(badResponse));

    EXPECT_TRUE(loadBalancer_->fetchLedger(sequence_, getObjects_, getObjectNeighbors_, std::chrono::milliseconds{1})
                    .has_value());
}

struct LoadBalancerForwardToRippledTests : LoadBalancerConstructorTests, SyncAsioContextTest {
    LoadBalancerForwardToRippledTests()
    {
        util::Random::setSeed(0);
        EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled).WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(0), run);
        EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled).WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    }

    boost::json::object const request_{{"request", "value"}};
    std::optional<std::string> const clientIP_ = "some_ip";
    boost::json::object const response_{{"response", "other_value"}};
};

TEST_F(LoadBalancerForwardToRippledTests, forward)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();
    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(request_, clientIP_, LoadBalancer::ADMIN_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(response_));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(loadBalancer->forwardToRippled(request_, clientIP_, true, yield), response_);
    });
}

TEST_F(LoadBalancerForwardToRippledTests, forwardWithXUserHeader)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();
    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(request_, clientIP_, LoadBalancer::USER_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(response_));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(loadBalancer->forwardToRippled(request_, clientIP_, false, yield), response_);
    });
}

TEST_F(LoadBalancerForwardToRippledTests, source0Fails)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();
    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(request_, clientIP_, LoadBalancer::USER_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(std::unexpected{rpc::ClioError::etlCONNECTION_ERROR}));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1),
        forwardToRippled(request_, clientIP_, LoadBalancer::USER_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(response_));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(loadBalancer->forwardToRippled(request_, clientIP_, false, yield), response_);
    });
}

struct LoadBalancerForwardToRippledErrorTestBundle {
    std::string testName;
    rpc::ClioError firstSourceError;
    rpc::ClioError secondSourceError;
    rpc::ClioError responseExpectedError;
};

struct LoadBalancerForwardToRippledErrorTests
    : LoadBalancerForwardToRippledTests,
      testing::WithParamInterface<LoadBalancerForwardToRippledErrorTestBundle> {};

INSTANTIATE_TEST_SUITE_P(
    LoadBalancerForwardToRippledErrorTests,
    LoadBalancerForwardToRippledErrorTests,
    testing::Values(
        LoadBalancerForwardToRippledErrorTestBundle{
            "ConnectionError_RequestError",
            rpc::ClioError::etlCONNECTION_ERROR,
            rpc::ClioError::etlREQUEST_ERROR,
            rpc::ClioError::etlREQUEST_ERROR
        },
        LoadBalancerForwardToRippledErrorTestBundle{
            "RequestError_RequestTimeout",
            rpc::ClioError::etlREQUEST_ERROR,
            rpc::ClioError::etlREQUEST_TIMEOUT,
            rpc::ClioError::etlREQUEST_TIMEOUT
        },
        LoadBalancerForwardToRippledErrorTestBundle{
            "RequestTimeout_InvalidResponse",
            rpc::ClioError::etlREQUEST_TIMEOUT,
            rpc::ClioError::etlINVALID_RESPONSE,
            rpc::ClioError::etlINVALID_RESPONSE
        },
        LoadBalancerForwardToRippledErrorTestBundle{
            "BothRequestTimeout",
            rpc::ClioError::etlREQUEST_TIMEOUT,
            rpc::ClioError::etlREQUEST_TIMEOUT,
            rpc::ClioError::etlREQUEST_TIMEOUT
        },
        LoadBalancerForwardToRippledErrorTestBundle{
            "InvalidResponse_RequestError",
            rpc::ClioError::etlINVALID_RESPONSE,
            rpc::ClioError::etlREQUEST_ERROR,
            rpc::ClioError::etlINVALID_RESPONSE
        }
    ),
    tests::util::NameGenerator
);

TEST_P(LoadBalancerForwardToRippledErrorTests, bothSourcesFail)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();
    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(request_, clientIP_, LoadBalancer::USER_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(std::unexpected{GetParam().firstSourceError}));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1),
        forwardToRippled(request_, clientIP_, LoadBalancer::USER_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(std::unexpected{GetParam().secondSourceError}));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const response = loadBalancer->forwardToRippled(request_, clientIP_, false, yield);
        ASSERT_FALSE(response);
        EXPECT_EQ(response.error(), GetParam().responseExpectedError);
    });
}

TEST_F(LoadBalancerForwardToRippledTests, forwardingCacheEnabled)
{
    configJson_.as_object()["forwarding"] = boost::json::object{{"cache_timeout", 10.}};
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();

    auto const request = boost::json::object{{"command", "server_info"}};

    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(request, clientIP_, LoadBalancer::USER_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(response_));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
    });
}

TEST_F(LoadBalancerForwardToRippledTests, forwardingCacheDisabledOnLedgerClosedHookCalled)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();
    EXPECT_NO_THROW(sourceFactory_.callbacksAt(0).onLedgerClosed());
}

TEST_F(LoadBalancerForwardToRippledTests, onLedgerClosedHookInvalidatesCache)
{
    configJson_.as_object()["forwarding"] = boost::json::object{{"cache_timeout", 10.}};
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();

    auto const request = boost::json::object{{"command", "server_info"}};

    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(request, clientIP_, LoadBalancer::USER_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(response_));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1),
        forwardToRippled(request, clientIP_, LoadBalancer::USER_FORWARDING_X_USER_VALUE, testing::_)
    )
        .WillOnce(Return(boost::json::object{}));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
        sourceFactory_.callbacksAt(0).onLedgerClosed();
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), boost::json::object{});
    });
}

struct LoadBalancerToJsonTests : LoadBalancerOnConnectHookTests {};

TEST_F(LoadBalancerToJsonTests, toJson)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), toJson).WillOnce(Return(boost::json::object{{"source1", "value1"}}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), toJson).WillOnce(Return(boost::json::object{{"source2", "value2"}}));

    auto const expectedJson =
        boost::json::array({boost::json::object{{"source1", "value1"}}, boost::json::object{{"source2", "value2"}}});
    EXPECT_EQ(loadBalancer_->toJson(), expectedJson);
}
