#include "etl/InitialLoadObserverInterface.hpp"
#include "etl/LoadBalancer.hpp"
#include "etl/LoadBalancerInterface.hpp"
#include "etl/Models.hpp"
#include "etl/Source.hpp"
#include "rpc/Errors.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockRandomGenerator.hpp"
#include "util/MockSource.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/NameGenerator.hpp"
#include "util/config/Array.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "util/prometheus/Counter.hpp"

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
using namespace util::prometheus;

namespace {

constinit auto const kTWO_SOURCES_LEDGER_RESPONSE = R"JSON({
    "etl_sources": [
        {
            "ip": "127.0.0.1",
            "ws_port": "5005",
            "grpc_port": "source1"
        },
        {
            "ip": "127.0.0.1",
            "ws_port": "5005",
            "grpc_port": "source2"
        }
    ]
})JSON";

constinit auto const kTHREE_SOURCES_LEDGER_RESPONSE = R"JSON({
    "etl_sources": [
        {
            "ip": "127.0.0.1",
            "ws_port": "5005",
            "grpc_port": "source1"
        },
        {
            "ip": "127.0.0.1",
            "ws_port": "5005",
            "grpc_port": "source2"
        },
        {
            "ip": "127.0.0.1",
            "ws_port": "5005",
            "grpc_port": "source3"
        }
    ]
})JSON";

inline ClioConfigDefinition
getParseLoadBalancerConfig(boost::json::value val)
{
    ClioConfigDefinition config{
        {{"forwarding.cache_timeout",
          ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(
              gValidatePositiveDouble
          )},
         {"forwarding.request_timeout",
          ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(
              gValidatePositiveDouble
          )},
         {"allow_no_etl", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
         {"etl_sources.[].ip",
          Array{ConfigValue{ConfigType::String}.optional().withConstraint(gValidateIp)}},
         {"etl_sources.[].ws_port",
          Array{ConfigValue{ConfigType::String}.optional().withConstraint(gValidatePort)}},
         {"etl_sources.[].grpc_port", Array{ConfigValue{ConfigType::String}.optional()}},
         {"num_markers",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateNumMarkers)}}
    };

    auto const errors = config.parse(ConfigFileJson{val.as_object()});
    [&]() { ASSERT_FALSE(errors.has_value()); }();

    return config;
}

struct InitialLoadObserverMock : etl::InitialLoadObserverInterface {
    MOCK_METHOD(
        void,
        onInitialLoadGotMoreObjects,
        (uint32_t, std::vector<etl::model::Object> const&, std::optional<std::string>),
        (override)
    );

    void
    onInitialLoadGotMoreObjects(uint32_t seq, std::vector<etl::model::Object> const& data)
    {
        onInitialLoadGotMoreObjects(seq, data, std::nullopt);
    }
};

}  // namespace

struct LoadBalancerConstructorTests : util::prometheus::WithPrometheus, MockBackendTestStrict {
    std::unique_ptr<LoadBalancer>
    makeLoadBalancer()
    {
        auto const cfg = getParseLoadBalancerConfig(configJson_);

        auto randomGenerator = std::make_unique<MockRandomGenerator>();
        randomGenerator_ = randomGenerator.get();

        return std::make_unique<LoadBalancer>(
            cfg,
            ioContext_,
            backend_,
            subscriptionManager_,
            std::move(randomGenerator),
            networkManager_,
            [this](auto&&... args) -> SourcePtr {
                return sourceFactory_(std::forward<decltype(args)>(args)...);
            }
        );
    }

protected:
    MockRandomGenerator* randomGenerator_ = nullptr;
    StrictMockSubscriptionManagerSharedPtr subscriptionManager_;
    StrictMockNetworkValidatedLedgersPtr networkManager_;
    StrictMockSourceFactory sourceFactory_{2};
    boost::asio::io_context ioContext_;
    boost::json::value configJson_ = boost::json::parse(kTWO_SOURCES_LEDGER_RESPONSE);
};

TEST_F(LoadBalancerConstructorTests, construct)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, forwardingTimeoutPassedToSourceFactory)
{
    auto const forwardingTimeout = 10;
    configJson_.as_object()["forwarding"] =
        boost::json::object{{"cache_timeout", float{forwardingTimeout}}};
    EXPECT_CALL(
        sourceFactory_,
        makeSource(
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
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_AllSourcesFail)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlConnectionError}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlConnectionError}));
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
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlConnectionError}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_Source0Fails1OK)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlConnectionError}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_DifferentNetworkID)
{
    auto const source1Json =
        boost::json::parse(R"JSON({"result": {"info": {"network_id": 0}}})JSON");
    auto const source2Json =
        boost::json::parse(R"JSON({"result": {"info": {"network_id": 1}}})JSON");

    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(source1Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(source2Json.as_object()));
    EXPECT_THROW({ makeLoadBalancer(); }, std::logic_error);
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_AllSourcesFailButAllowNoEtlIsTrue)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlConnectionError}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);

    configJson_.as_object()["allow_no_etl"] = true;
    makeLoadBalancer();
}

TEST_F(LoadBalancerConstructorTests, fetchETLState_DifferentNetworkIDButAllowNoEtlIsTrue)
{
    auto const source1Json =
        boost::json::parse(R"JSON({"result": {"info": {"network_id": 0}}})JSON");
    auto const source2Json =
        boost::json::parse(R"JSON({"result": {"info": {"network_id": 1}}})JSON");
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(source1Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(source2Json.as_object()));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);

    configJson_.as_object()["allow_no_etl"] = true;
    makeLoadBalancer();
}

struct LoadBalancerOnConnectHookTests : LoadBalancerConstructorTests {
    LoadBalancerOnConnectHookTests()
    {
        EXPECT_CALL(sourceFactory_, makeSource).Times(2);
        EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
            .WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(0), run);
        EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
            .WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(1), run);
        loadBalancer_ = makeLoadBalancer();
    }

protected:
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

struct LoadBalancerStopTests : LoadBalancerOnConnectHookTests, SyncAsioContextTest {};

TEST_F(LoadBalancerStopTests, stopCallsSourcesStop)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), stop);
    EXPECT_CALL(sourceFactory_.sourceAt(1), stop);
    runSyncOperation([this](boost::asio::yield_context yield) { loadBalancer_->stop(yield); });
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
    sourceFactory_.callbacksAt(0).onDisconnect(true);
}

TEST_F(LoadBalancerOnDisconnectHookTests, source1Disconnects)
{
    sourceFactory_.callbacksAt(1).onDisconnect(false);
}

TEST_F(LoadBalancerOnDisconnectHookTests, source0DisconnectsAndConnectsBack)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(true));
    sourceFactory_.callbacksAt(0).onDisconnect(true);

    sourceFactory_.callbacksAt(0).onConnect();
}

TEST_F(LoadBalancerOnDisconnectHookTests, source1DisconnectsAndConnectsBack)
{
    sourceFactory_.callbacksAt(1).onDisconnect(false);
    sourceFactory_.callbacksAt(1).onConnect();
}

TEST_F(LoadBalancerOnConnectHookTests, bothSourcesDisconnectAndConnectBack)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(0), setForwarding(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), isConnected()).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), setForwarding(false));
    sourceFactory_.callbacksAt(0).onDisconnect(true);
    sourceFactory_.callbacksAt(1).onDisconnect(false);

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
        configJson_ = boost::json::parse(kTHREE_SOURCES_LEDGER_RESPONSE);

        EXPECT_CALL(sourceFactory_, makeSource).Times(3);
        EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
            .WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(0), run);
        EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
            .WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(1), run);
        EXPECT_CALL(sourceFactory_.sourceAt(2), forwardToRippled)
            .WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(2), run);
        loadBalancer_ = makeLoadBalancer();
    }

protected:
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
    sourceFactory_.callbacksAt(0).onDisconnect(false);
}

struct LoadBalancerLoadInitialLedgerTests : LoadBalancerOnConnectHookTests {
protected:
    uint32_t const sequence_ = 123;
    uint32_t const numMarkers_ = 16;
    InitialLedgerLoadResult const response_{std::vector<std::string>{"1", "2", "3"}};
    testing::StrictMock<InitialLoadObserverMock> observer_;
};

TEST_F(LoadBalancerLoadInitialLedgerTests, load)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), loadInitialLedger(sequence_, numMarkers_, testing::_))
        .WillOnce(Return(response_));

    EXPECT_EQ(
        loadBalancer_->loadInitialLedger(sequence_, observer_, std::chrono::milliseconds{1}),
        response_.value()
    );
}

TEST_F(LoadBalancerLoadInitialLedgerTests, load_source0DoesntHaveLedger)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), loadInitialLedger(sequence_, numMarkers_, testing::_))
        .WillOnce(Return(response_));

    EXPECT_EQ(
        loadBalancer_->loadInitialLedger(sequence_, observer_, std::chrono::milliseconds{1}),
        response_.value()
    );
}

TEST_F(LoadBalancerLoadInitialLedgerTests, load_bothSourcesDontHaveLedger)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_))
        .Times(2)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_))
        .WillOnce(Return(false))
        .WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), loadInitialLedger(sequence_, numMarkers_, testing::_))
        .WillOnce(Return(response_));

    EXPECT_EQ(
        loadBalancer_->loadInitialLedger(sequence_, observer_, std::chrono::milliseconds{1}),
        response_.value()
    );
}

TEST_F(LoadBalancerLoadInitialLedgerTests, load_source0ReturnsStatusFalse)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), loadInitialLedger(sequence_, numMarkers_, testing::_))
        .WillOnce(Return(std::unexpected{InitialLedgerLoadError::Errored}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(1), loadInitialLedger(sequence_, numMarkers_, testing::_))
        .WillOnce(Return(response_));

    EXPECT_EQ(
        loadBalancer_->loadInitialLedger(sequence_, observer_, std::chrono::milliseconds{1}),
        response_.value()
    );
}

struct LoadBalancerLoadInitialLedgerCustomNumMarkersTests : LoadBalancerConstructorTests {
protected:
    uint32_t const numMarkers_ = 16;
    uint32_t const sequence_ = 123;
    InitialLedgerLoadResult const response_{std::vector<std::string>{"1", "2", "3"}};
    testing::StrictMock<InitialLoadObserverMock> observer_;
};

TEST_F(LoadBalancerLoadInitialLedgerCustomNumMarkersTests, loadInitialLedger)
{
    configJson_.as_object()["num_markers"] = numMarkers_;

    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(0), run);
    EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
        .WillOnce(Return(boost::json::object{}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    auto loadBalancer = makeLoadBalancer();

    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(sourceFactory_.sourceAt(0), loadInitialLedger(sequence_, numMarkers_, testing::_))
        .WillOnce(Return(response_));

    EXPECT_EQ(
        loadBalancer->loadInitialLedger(sequence_, observer_, std::chrono::milliseconds{1}),
        response_.value()
    );
}

struct LoadBalancerFetchLegerTests : LoadBalancerOnConnectHookTests {
    LoadBalancerFetchLegerTests()
    {
        response_.second.set_validated(true);
    }

protected:
    uint32_t const sequence_ = 123;
    bool const getObjects_ = true;
    bool const getObjectNeighbors_ = false;
    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse> response_ =
        std::make_pair(grpc::Status::OK, org::xrpl::rpc::v1::GetLedgerResponse{});
};

TEST_F(LoadBalancerFetchLegerTests, fetch)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(
        sourceFactory_.sourceAt(0), fetchLedger(sequence_, getObjects_, getObjectNeighbors_)
    )
        .WillOnce(Return(response_));

    EXPECT_TRUE(
        loadBalancer_->fetchLedger(sequence_, getObjects_, getObjectNeighbors_).has_value()
    );
}

TEST_F(LoadBalancerFetchLegerTests, fetch_Source0ReturnsBadStatus)
{
    auto source0Response = response_;
    source0Response.first = grpc::Status::CANCELLED;

    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(
        sourceFactory_.sourceAt(0), fetchLedger(sequence_, getObjects_, getObjectNeighbors_)
    )
        .WillOnce(Return(source0Response));

    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1), fetchLedger(sequence_, getObjects_, getObjectNeighbors_)
    )
        .WillOnce(Return(response_));

    EXPECT_TRUE(
        loadBalancer_->fetchLedger(sequence_, getObjects_, getObjectNeighbors_).has_value()
    );
}

TEST_F(LoadBalancerFetchLegerTests, fetch_Source0ReturnsNotValidated)
{
    auto source0Response = response_;
    source0Response.second.set_validated(false);

    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(
        sourceFactory_.sourceAt(0), fetchLedger(sequence_, getObjects_, getObjectNeighbors_)
    )
        .WillOnce(Return(source0Response));

    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1), fetchLedger(sequence_, getObjects_, getObjectNeighbors_)
    )
        .WillOnce(Return(response_));

    EXPECT_TRUE(
        loadBalancer_->fetchLedger(sequence_, getObjects_, getObjectNeighbors_).has_value()
    );
}

TEST_F(LoadBalancerFetchLegerTests, fetch_bothSourcesFail)
{
    auto badResponse = response_;
    badResponse.second.set_validated(false);

    EXPECT_CALL(sourceFactory_.sourceAt(0), hasLedger(sequence_))
        .Times(2)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(
        sourceFactory_.sourceAt(0), fetchLedger(sequence_, getObjects_, getObjectNeighbors_)
    )
        .WillOnce(Return(badResponse))
        .WillOnce(Return(response_));

    EXPECT_CALL(sourceFactory_.sourceAt(1), hasLedger(sequence_)).WillOnce(Return(true));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1), fetchLedger(sequence_, getObjects_, getObjectNeighbors_)
    )
        .WillOnce(Return(badResponse));

    EXPECT_TRUE(
        loadBalancer_
            ->fetchLedger(sequence_, getObjects_, getObjectNeighbors_, std::chrono::milliseconds{1})
            .has_value()
    );
}

struct LoadBalancerForwardToRippledTests : LoadBalancerConstructorTests, SyncAsioContextTest {
    LoadBalancerForwardToRippledTests()
    {
        EXPECT_CALL(sourceFactory_.sourceAt(0), forwardToRippled)
            .WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(0), run);
        EXPECT_CALL(sourceFactory_.sourceAt(1), forwardToRippled)
            .WillOnce(Return(boost::json::object{}));
        EXPECT_CALL(sourceFactory_.sourceAt(1), run);
    }

protected:
    boost::json::object const request_{{"command", "value"}};
    std::optional<std::string> const clientIP_ = "some_ip";
    boost::json::object const response_{{"response", "other_value"}};
};

TEST_F(LoadBalancerForwardToRippledTests, forward)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();
    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(
            request_, clientIP_, LoadBalancer::kADMIN_FORWARDING_X_USER_VALUE, testing::_
        )
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
        forwardToRippled(
            request_, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
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
        forwardToRippled(
            request_, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
    )
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlConnectionError}));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1),
        forwardToRippled(
            request_, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
    )
        .WillOnce(Return(response_));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(loadBalancer->forwardToRippled(request_, clientIP_, false, yield), response_);
    });
}

struct LoadBalancerForwardToRippledPrometheusTests : LoadBalancerForwardToRippledTests,
                                                     WithMockPrometheus {};

TEST_F(LoadBalancerForwardToRippledPrometheusTests, forwardingCacheEnabled)
{
    configJson_.as_object()["forwarding"] = boost::json::object{{"cache_timeout", 10.}};
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();

    auto const request = boost::json::object{{"command", "server_info"}};

    auto& cacheHitCounter = makeMock<CounterInt>("forwarding_cache_hit_counter", "");
    auto& cacheMissCounter = makeMock<CounterInt>("forwarding_cache_miss_counter", "");
    auto& successDurationCounter =
        makeMock<CounterInt>("forwarding_duration_milliseconds_counter", "{status=\"success\"}");

    EXPECT_CALL(cacheMissCounter, add(1));
    EXPECT_CALL(cacheHitCounter, add(1)).Times(3);
    EXPECT_CALL(successDurationCounter, add(testing::_));

    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(
            request, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
    )
        .WillOnce(Return(response_));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
    });
}

TEST_F(LoadBalancerForwardToRippledPrometheusTests, source0Fails)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();

    auto& cacheMissCounter = makeMock<CounterInt>("forwarding_cache_miss_counter", "");
    auto& retriesCounter = makeMock<CounterInt>("forwarding_retries_counter", "");
    auto& successDurationCounter =
        makeMock<CounterInt>("forwarding_duration_milliseconds_counter", "{status=\"success\"}");
    auto& failDurationCounter =
        makeMock<CounterInt>("forwarding_duration_milliseconds_counter", "{status=\"fail\"}");

    EXPECT_CALL(cacheMissCounter, add(1));
    EXPECT_CALL(retriesCounter, add(1));
    EXPECT_CALL(successDurationCounter, add(testing::_));
    EXPECT_CALL(failDurationCounter, add(testing::_));

    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(
            request_, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
    )
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlConnectionError}));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1),
        forwardToRippled(
            request_, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
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
    rpc::CombinedError responseExpectedError;
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
            rpc::ClioError::EtlConnectionError,
            rpc::ClioError::EtlRequestError,
            rpc::ClioError::EtlRequestError
        },
        LoadBalancerForwardToRippledErrorTestBundle{
            "RequestError_RequestTimeout",
            rpc::ClioError::EtlRequestError,
            rpc::ClioError::EtlRequestTimeout,
            rpc::ClioError::EtlRequestTimeout
        },
        LoadBalancerForwardToRippledErrorTestBundle{
            "RequestTimeout_InvalidResponse",
            rpc::ClioError::EtlRequestTimeout,
            rpc::ClioError::EtlInvalidResponse,
            rpc::ClioError::EtlInvalidResponse
        },
        LoadBalancerForwardToRippledErrorTestBundle{
            "BothRequestTimeout",
            rpc::ClioError::EtlRequestTimeout,
            rpc::ClioError::EtlRequestTimeout,
            rpc::ClioError::EtlRequestTimeout
        },
        LoadBalancerForwardToRippledErrorTestBundle{
            "InvalidResponse_RequestError",
            rpc::ClioError::EtlInvalidResponse,
            rpc::ClioError::EtlRequestError,
            rpc::ClioError::EtlInvalidResponse
        }
    ),
    tests::util::kNAME_GENERATOR
);

TEST_P(LoadBalancerForwardToRippledErrorTests, bothSourcesFail)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();
    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(
            request_, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
    )
        .WillOnce(Return(std::unexpected{GetParam().firstSourceError}));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1),
        forwardToRippled(
            request_, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
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
        forwardToRippled(
            request, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
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

    EXPECT_CALL(*randomGenerator_, uniform(0, 1)).WillOnce(Return(0)).WillOnce(Return(1));

    EXPECT_CALL(
        sourceFactory_.sourceAt(0),
        forwardToRippled(
            request, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
    )
        .WillOnce(Return(response_));
    EXPECT_CALL(
        sourceFactory_.sourceAt(1),
        forwardToRippled(
            request, clientIP_, LoadBalancer::kUSER_FORWARDING_X_USER_VALUE, testing::_
        )
    )
        .WillOnce(Return(boost::json::object{}));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
        EXPECT_EQ(loadBalancer->forwardToRippled(request, clientIP_, false, yield), response_);
        sourceFactory_.callbacksAt(0).onLedgerClosed();
        EXPECT_EQ(
            loadBalancer->forwardToRippled(request, clientIP_, false, yield), boost::json::object{}
        );
    });
}

TEST_F(LoadBalancerForwardToRippledTests, commandLineMissing)
{
    EXPECT_CALL(sourceFactory_, makeSource).Times(2);
    auto loadBalancer = makeLoadBalancer();

    auto const request = boost::json::object{{"command2", "server_info"}};

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_EQ(
            loadBalancer->forwardToRippled(request, clientIP_, false, yield).error(),
            rpc::CombinedError{rpc::ClioError::RpcCommandIsMissing}
        );
    });
}

struct LoadBalancerToJsonTests : LoadBalancerOnConnectHookTests {};

TEST_F(LoadBalancerToJsonTests, toJson)
{
    EXPECT_CALL(sourceFactory_.sourceAt(0), toJson)
        .WillOnce(Return(boost::json::object{{"source1", "value1"}}));
    EXPECT_CALL(sourceFactory_.sourceAt(1), toJson)
        .WillOnce(Return(boost::json::object{{"source2", "value2"}}));

    auto const expectedJson = boost::json::array(
        {boost::json::object{{"source1", "value1"}}, boost::json::object{{"source2", "value2"}}}
    );
    EXPECT_EQ(loadBalancer_->toJson(), expectedJson);
}
