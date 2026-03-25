#include "app/Stopper.hpp"
#include "cluster/Concepts.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackend.hpp"
#include "util/MockETLService.hpp"
#include "util/MockLoadBalancer.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "web/interface/Concepts.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

using namespace app;

struct StopperTest : virtual public ::testing::Test {
protected:
    // Order here is important, stopper_ should die before mockCallback_, otherwise UB
    testing::StrictMock<testing::MockFunction<void(boost::asio::yield_context)>> mockCallback_;
    testing::StrictMock<testing::MockFunction<void()>> mockCompleteCallback_;
    Stopper stopper_;
};

TEST_F(StopperTest, stopCallsCallback)
{
    stopper_.setOnStop(mockCallback_.AsStdFunction());
    EXPECT_CALL(mockCallback_, Call);
    stopper_.stop();
}

TEST_F(StopperTest, stopCalledMultipleTimes)
{
    stopper_.setOnStop(mockCallback_.AsStdFunction());
    EXPECT_CALL(mockCallback_, Call);
    stopper_.stop();
    stopper_.stop();
    stopper_.stop();
    stopper_.stop();
}

TEST_F(StopperTest, stopCallsCompletionCallback)
{
    stopper_.setOnStop(mockCallback_.AsStdFunction());
    stopper_.setOnComplete(mockCompleteCallback_.AsStdFunction());
    EXPECT_CALL(mockCallback_, Call);
    EXPECT_CALL(mockCompleteCallback_, Call);
    stopper_.stop();
}

TEST_F(StopperTest, stopWithoutCompletionCallback)
{
    stopper_.setOnStop(mockCallback_.AsStdFunction());
    EXPECT_CALL(mockCallback_, Call);
    stopper_.stop();
}

struct StopperMakeCallbackTest : util::prometheus::WithPrometheus, SyncAsioContextTest {
    struct ServerMock : web::ServerTag {
        MOCK_METHOD(void, stop, (boost::asio::yield_context), ());
    };

    struct MockLedgerCacheSaver {
        MOCK_METHOD(void, save, ());
        MOCK_METHOD(void, waitToFinish, ());
    };

    struct MockClusterCommunicationService : cluster::ClusterCommunicationServiceTag {
        MOCK_METHOD(void, stop, (), ());
    };

protected:
    testing::StrictMock<ServerMock> serverMock_;
    testing::StrictMock<MockLoadBalancer> loadBalancerMock_;
    testing::StrictMock<MockETLService> etlServiceMock_;
    testing::StrictMock<MockSubscriptionManager> subscriptionManagerMock_;
    testing::StrictMock<MockBackend> backendMock_{util::config::ClioConfigDefinition{}};
    testing::StrictMock<MockLedgerCacheSaver> cacheSaverMock_;
    testing::StrictMock<MockClusterCommunicationService> clusterCommunicationServiceMock_;
    boost::asio::io_context ioContextToStop_;

    bool
    isContextStopped() const
    {
        return ioContextToStop_.stopped();
    }
};

TEST_F(StopperMakeCallbackTest, makeCallbackTest)
{
    auto contextWorkGuard = boost::asio::make_work_guard(ioContextToStop_);
    std::thread t{[this]() { ioContextToStop_.run(); }};

    auto callback = Stopper::makeOnStopCallback(
        serverMock_,
        loadBalancerMock_,
        etlServiceMock_,
        subscriptionManagerMock_,
        backendMock_,
        cacheSaverMock_,
        clusterCommunicationServiceMock_,
        ioContextToStop_
    );

    testing::Sequence const s1, s2;
    EXPECT_CALL(cacheSaverMock_, save).InSequence(s1).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });
    EXPECT_CALL(serverMock_, stop).InSequence(s1).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });
    EXPECT_CALL(loadBalancerMock_, stop).InSequence(s2).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });
    EXPECT_CALL(clusterCommunicationServiceMock_, stop).InSequence(s1, s2).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });
    EXPECT_CALL(etlServiceMock_, stop).InSequence(s1, s2).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });
    EXPECT_CALL(subscriptionManagerMock_, stop).InSequence(s1, s2).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });
    EXPECT_CALL(backendMock_, waitForWritesToFinish).InSequence(s1, s2).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });
    EXPECT_CALL(cacheSaverMock_, waitToFinish).InSequence(s1).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });

    runSpawn([&](boost::asio::yield_context yield) {
        callback(yield);
        EXPECT_TRUE(isContextStopped());
    });

    t.join();
}
