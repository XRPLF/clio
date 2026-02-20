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
#include <boost/json/value_to.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
    testing::StrictMock<
        testing::MockFunction<void(ClioNode::CUuid, std::shared_ptr<Backend::ClusterData const>)>>
        callbackMock;
    std::binary_semaphore semaphore{0};

    class SemaphoreReleaseGuard {
        std::binary_semaphore& semaphore_;

    public:
        SemaphoreReleaseGuard(std::binary_semaphore& s) : semaphore_(s)
        {
        }
        ~SemaphoreReleaseGuard()
        {
            semaphore_.release();
        }
    };
};

TEST_F(ClusterBackendTest, SubscribeToNewState)
{
    Backend clusterBackend{
        ctx,
        backend_,
        std::move(writerState),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    };

    clusterBackend.subscribeToNewState(callbackMock.AsStdFunction());

    EXPECT_CALL(*backend_, fetchClioNodesData)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(BackendInterface::ClioNodesDataFetchResult{}));
    EXPECT_CALL(*backend_, writeNodeMessage).Times(testing::AtLeast(1));
    EXPECT_CALL(writerStateRef, isReadOnly)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(callbackMock, Call)
        .Times(testing::AtLeast(1))
        .WillRepeatedly([this](
                            ClioNode::CUuid selfId,
                            std::shared_ptr<Backend::ClusterData const> clusterData
                        ) {
            SemaphoreReleaseGuard const guard{semaphore};
            ASSERT_TRUE(clusterData->has_value());
            EXPECT_EQ(clusterData->value().size(), 1);
            auto const& nodeData = clusterData->value().front();
            EXPECT_EQ(nodeData.uuid, selfId);
            EXPECT_EQ(nodeData.dbRole, ClioNode::DbRole::ReadOnly);
            EXPECT_LE(nodeData.updateTime, std::chrono::system_clock::now());
        });

    clusterBackend.run();
    semaphore.acquire();
}

TEST_F(ClusterBackendTest, Stop)
{
    Backend clusterBackend{
        ctx,
        backend_,
        std::move(writerState),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    };

    EXPECT_CALL(*backend_, fetchClioNodesData)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(BackendInterface::ClioNodesDataFetchResult{}));
    EXPECT_CALL(*backend_, writeNodeMessage).Times(testing::AtLeast(1));
    EXPECT_CALL(writerStateRef, isReadOnly)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(true));

    clusterBackend.run();
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    clusterBackend.stop();

    testing::Mock::VerifyAndClearExpectations(&(*backend_));
    // Wait to make sure there is no new calls of mockDbBackend
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
}

TEST_F(ClusterBackendTest, FetchClioNodesDataThrowsException)
{
    Backend clusterBackend{
        ctx,
        backend_,
        std::move(writerState),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    };

    clusterBackend.subscribeToNewState(callbackMock.AsStdFunction());

    EXPECT_CALL(*backend_, fetchClioNodesData)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Throw(std::runtime_error("Database connection failed")));
    EXPECT_CALL(*backend_, writeNodeMessage).Times(testing::AtLeast(1));
    EXPECT_CALL(writerStateRef, isReadOnly)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(callbackMock, Call)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            [this](ClioNode::CUuid, std::shared_ptr<Backend::ClusterData const> clusterData) {
                SemaphoreReleaseGuard const guard{semaphore};
                ASSERT_FALSE(clusterData->has_value());
                EXPECT_EQ(clusterData->error(), "Failed to fetch Clio nodes data");
            }
        );

    clusterBackend.run();
    semaphore.acquire();
}

TEST_F(ClusterBackendTest, FetchClioNodesDataReturnsDataWithOtherNodes)
{
    Backend clusterBackend{
        ctx,
        backend_,
        std::move(writerState),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    };

    clusterBackend.subscribeToNewState(callbackMock.AsStdFunction());

    auto const otherUuid = boost::uuids::random_generator{}();
    auto const otherNodeJson = R"JSON({
        "db_role": 3,
        "update_time": "2025-01-15T10:30:00Z"
    })JSON";

    EXPECT_CALL(*backend_, fetchClioNodesData)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            testing::Return(
                BackendInterface::ClioNodesDataFetchResult{
                    std::vector<std::pair<boost::uuids::uuid, std::string>>{
                        {otherUuid, otherNodeJson}
                    }
                }
            )
        );
    EXPECT_CALL(*backend_, writeNodeMessage).Times(testing::AtLeast(1));
    EXPECT_CALL(writerStateRef, isReadOnly)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(writerStateRef, isFallback)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(writerStateRef, isLoadingCache)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(writerStateRef, isWriting)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(callbackMock, Call)
        .Times(testing::AtLeast(1))
        .WillRepeatedly([&](ClioNode::CUuid selfId,
                            std::shared_ptr<Backend::ClusterData const> clusterData) {
            SemaphoreReleaseGuard const guard{semaphore};
            ASSERT_TRUE(clusterData->has_value()) << clusterData->error();
            EXPECT_EQ(clusterData->value().size(), 2);
            EXPECT_EQ(selfId, clusterBackend.selfId());

            bool foundSelf = false;
            bool foundOther = false;

            for (auto const& node : clusterData->value()) {
                if (*node.uuid == *selfId) {
                    foundSelf = true;
                    EXPECT_EQ(node.dbRole, ClioNode::DbRole::NotWriter);
                } else if (*node.uuid == otherUuid) {
                    foundOther = true;
                    EXPECT_EQ(node.dbRole, ClioNode::DbRole::Writer);
                }
                EXPECT_LE(node.updateTime, std::chrono::system_clock::now());
            }

            EXPECT_TRUE(foundSelf);
            EXPECT_TRUE(foundOther);
        });

    clusterBackend.run();
    semaphore.acquire();
}

TEST_F(ClusterBackendTest, FetchClioNodesDataReturnsOnlySelfData)
{
    Backend clusterBackend{
        ctx,
        backend_,
        std::move(writerState),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    };

    clusterBackend.subscribeToNewState(callbackMock.AsStdFunction());

    auto const selfNodeJson = R"JSON({
        "db_role": 1,
        "update_time": "2025-01-16T10:30:00Z"
    })JSON";

    EXPECT_CALL(*backend_, fetchClioNodesData).Times(testing::AtLeast(1)).WillRepeatedly([&]() {
        return BackendInterface::ClioNodesDataFetchResult{
            std::vector<std::pair<boost::uuids::uuid, std::string>>{
                {*clusterBackend.selfId(), selfNodeJson}
            }
        };
    });
    EXPECT_CALL(*backend_, writeNodeMessage).Times(testing::AtLeast(1));
    EXPECT_CALL(writerStateRef, isReadOnly)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(callbackMock, Call)
        .Times(testing::AtLeast(1))
        .WillRepeatedly([this](
                            ClioNode::CUuid selfId,
                            std::shared_ptr<Backend::ClusterData const> clusterData
                        ) {
            SemaphoreReleaseGuard const guard{semaphore};
            ASSERT_TRUE(clusterData->has_value());
            EXPECT_EQ(clusterData->value().size(), 1);
            auto const& nodeData = clusterData->value().front();
            EXPECT_EQ(nodeData.uuid, selfId);
            EXPECT_EQ(nodeData.dbRole, ClioNode::DbRole::ReadOnly);
            EXPECT_LE(nodeData.updateTime, std::chrono::system_clock::now());
        });

    clusterBackend.run();
    semaphore.acquire();
}

TEST_F(ClusterBackendTest, FetchClioNodesDataReturnsInvalidJson)
{
    Backend clusterBackend{
        ctx,
        backend_,
        std::move(writerState),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    };

    clusterBackend.subscribeToNewState(callbackMock.AsStdFunction());

    auto const otherUuid = boost::uuids::random_generator{}();
    auto const invalidJson = "{ invalid json";

    EXPECT_CALL(*backend_, fetchClioNodesData)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            testing::Return(
                BackendInterface::ClioNodesDataFetchResult{
                    std::vector<std::pair<boost::uuids::uuid, std::string>>{
                        {otherUuid, invalidJson}
                    }
                }
            )
        );
    EXPECT_CALL(*backend_, writeNodeMessage).Times(testing::AtLeast(1));
    EXPECT_CALL(writerStateRef, isReadOnly)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(callbackMock, Call)
        .Times(testing::AtLeast(1))
        .WillRepeatedly([this, invalidJson](
                            ClioNode::CUuid, std::shared_ptr<Backend::ClusterData const> clusterData
                        ) {
            SemaphoreReleaseGuard const guard{semaphore};
            ASSERT_FALSE(clusterData->has_value());
            EXPECT_THAT(clusterData->error(), testing::HasSubstr("Error parsing json from DB"));
            EXPECT_THAT(clusterData->error(), testing::HasSubstr(invalidJson));
        });

    clusterBackend.run();
    semaphore.acquire();
}

TEST_F(ClusterBackendTest, FetchClioNodesDataReturnsValidJsonButCannotConvertToClioNode)
{
    Backend clusterBackend{
        ctx,
        backend_,
        std::move(writerState),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    };

    clusterBackend.subscribeToNewState(callbackMock.AsStdFunction());

    auto const otherUuid = boost::uuids::random_generator{}();
    // Valid JSON but missing required field 'db_role'
    auto const validJsonMissingField = R"JSON({
        "update_time": "2025-01-16T10:30:00Z"
    })JSON";

    EXPECT_CALL(*backend_, fetchClioNodesData)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            testing::Return(
                BackendInterface::ClioNodesDataFetchResult{
                    std::vector<std::pair<boost::uuids::uuid, std::string>>{
                        {otherUuid, validJsonMissingField}
                    }
                }
            )
        );
    EXPECT_CALL(*backend_, writeNodeMessage).Times(testing::AtLeast(1));
    EXPECT_CALL(writerStateRef, isReadOnly)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(callbackMock, Call)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            [this](ClioNode::CUuid, std::shared_ptr<Backend::ClusterData const> clusterData) {
                SemaphoreReleaseGuard const guard{semaphore};
                ASSERT_FALSE(clusterData->has_value());
                EXPECT_THAT(
                    clusterData->error(), testing::HasSubstr("Error converting json to ClioNode")
                );
            }
        );

    clusterBackend.run();
    semaphore.acquire();
}

TEST_F(ClusterBackendTest, WriteNodeMessageWritesSelfDataWithRecentTimestampAndDbRole)
{
    Backend clusterBackend{
        ctx,
        backend_,
        std::move(writerState),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    };

    auto const beforeRun =
        std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());

    EXPECT_CALL(*backend_, fetchClioNodesData)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(BackendInterface::ClioNodesDataFetchResult{}));
    EXPECT_CALL(writerStateRef, isReadOnly)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(writerStateRef, isFallback)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(writerStateRef, isLoadingCache)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(writerStateRef, isWriting)
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*backend_, writeNodeMessage)
        .Times(testing::AtLeast(1))
        .WillRepeatedly([&](boost::uuids::uuid const& uuid, std::string message) {
            SemaphoreReleaseGuard const guard{semaphore};
            auto const afterWrite = std::chrono::system_clock::now();

            EXPECT_EQ(uuid, *clusterBackend.selfId());
            auto const json = boost::json::parse(message);
            auto const node = boost::json::try_value_to<ClioNode>(json);
            ASSERT_TRUE(node.has_value());
            EXPECT_EQ(node->dbRole, ClioNode::DbRole::NotWriter);
            EXPECT_GE(node->updateTime, beforeRun);
            EXPECT_LE(node->updateTime, afterWrite);
        });

    clusterBackend.run();
    semaphore.acquire();
}
