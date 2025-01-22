//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
#include "app/Stopper.hpp"
#include "etl/ETLService.hpp"
#include "etl/LoadBalancer.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockBackend.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "web/ng/Server.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

using namespace app;

struct StopperTest : NoLoggerFixture {
protected:
    // Order here is important, stopper_ should die before mockCallback_, otherwise UB
    testing::StrictMock<testing::MockFunction<void(boost::asio::yield_context)>> mockCallback_;
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

struct StopperMakeCallbackTest : util::prometheus::WithPrometheus, SyncAsioContextTest {
    struct ServerMock : web::ng::ServerTag {
        MOCK_METHOD(void, stop, (boost::asio::yield_context), ());
    };
    struct LoadBalancerMock : etl::LoadBalancerTag {
        MOCK_METHOD(void, stop, (boost::asio::yield_context), ());
    };
    struct ETLServiceMock : etl::ETLServiceTag {
        MOCK_METHOD(void, stop, (), ());
    };

protected:
    testing::StrictMock<ServerMock> serverMock_;
    testing::StrictMock<LoadBalancerMock> loadBalancerMock_;
    testing::StrictMock<ETLServiceMock> etlServiceMock_;
    testing::StrictMock<MockSubscriptionManager> subscriptionManagerMock_;
    testing::StrictMock<MockBackend> backendMock_{util::config::ClioConfigDefinition{}};
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
        serverMock_, loadBalancerMock_, etlServiceMock_, subscriptionManagerMock_, backendMock_, ioContextToStop_
    );

    testing::Sequence s1, s2;
    EXPECT_CALL(serverMock_, stop).InSequence(s1).WillOnce([this]() { EXPECT_FALSE(isContextStopped()); });
    EXPECT_CALL(loadBalancerMock_, stop).InSequence(s2).WillOnce([this]() { EXPECT_FALSE(isContextStopped()); });
    EXPECT_CALL(etlServiceMock_, stop).InSequence(s1, s2).WillOnce([this]() { EXPECT_FALSE(isContextStopped()); });
    EXPECT_CALL(subscriptionManagerMock_, stop).InSequence(s1, s2).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });
    EXPECT_CALL(backendMock_, waitForWritesToFinish).InSequence(s1, s2).WillOnce([this]() {
        EXPECT_FALSE(isContextStopped());
    });

    runSpawn([&](boost::asio::yield_context yield) {
        callback(yield);
        EXPECT_TRUE(isContextStopped());
    });

    t.join();
}
