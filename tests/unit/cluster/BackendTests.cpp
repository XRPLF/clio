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

#include "cluster/Backend.hpp"
#include "cluster/ClioNode.hpp"
#include "data/BackendInterface.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockWriterState.hpp"

#include <boost/asio/thread_pool.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <utility>

using namespace cluster;

struct ClusterBackendTest : util::prometheus::WithPrometheus, MockBackendTestStrict {
    ~ClusterBackendTest() override
    {
        ctx.stop();
        ctx.join();
    }

    boost::asio::thread_pool ctx;
    std::unique_ptr<MockWriterState> writerState = std::make_unique<MockWriterState>();
    MockWriterState& writerStateRef = *writerState;
    testing::StrictMock<testing::MockFunction<void(ClioNode::cUUID, std::shared_ptr<Backend::ClusterData const>)>>
        callbackMock;
};

TEST_F(ClusterBackendTest, Stop)
{
    Backend clusterBackend{
        ctx, backend_, std::move(writerState), std::chrono::milliseconds(1), std::chrono::milliseconds(1)
    };

    EXPECT_CALL(*backend_, fetchClioNodesData)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(BackendInterface::ClioNodesDataFetchResult{}));
    EXPECT_CALL(*backend_, writeNodeMessage).Times(testing::AtLeast(1));
    EXPECT_CALL(callbackMock, Call).Times(testing::AtLeast(1));
    EXPECT_CALL(writerStateRef, isReadOnly).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(true));

    clusterBackend.run();
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    clusterBackend.stop();

    testing::Mock::VerifyAndClearExpectations(&(*backend_));
    // Wait to make sure there is no new calls of mockDbBackend
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
}
