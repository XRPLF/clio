#include "cluster/Backend.hpp"

#include "cluster/ClioNode.hpp"
#include "data/BackendInterface.hpp"
#include "data/LedgerCacheLoadingState.hpp"
#include "etl/WriterState.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <fmt/format.h>

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

namespace cluster {

Backend::Backend(
    boost::asio::thread_pool& ctx,
    std::shared_ptr<data::BackendInterface> backend,
    std::unique_ptr<etl::WriterStateInterface const> writerState,
    std::unique_ptr<data::LedgerCacheLoadingStateInterface const> cacheLoadingState,
    std::chrono::steady_clock::duration readInterval,
    std::chrono::steady_clock::duration writeInterval
)
    : backend_(std::move(backend))
    , writerState_(std::move(writerState))
    , cacheLoadingState_(std::move(cacheLoadingState))
    , readerTask_(readInterval, ctx)
    , writerTask_(writeInterval, ctx)
    , selfUuid_(std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator{}()))
{
}

void
Backend::run()
{
    readerTask_.run([this](boost::asio::yield_context yield) {
        auto clusterData = doRead(yield);
        onNewState_(selfUuid_, std::make_shared<ClusterData>(std::move(clusterData)));
    });

    writerTask_.run([this]() { doWrite(); });
}

Backend::~Backend()
{
    stop();
}

void
Backend::stop()
{
    readerTask_.stop();
    writerTask_.stop();
}

ClioNode::CUuid
Backend::selfId() const
{
    return selfUuid_;
}

Backend::ClusterData
Backend::doRead(boost::asio::yield_context yield)
{
    BackendInterface::ClioNodesDataFetchResult expectedResult;
    try {
        expectedResult = backend_->fetchClioNodesData(yield);
    } catch (...) {
        expectedResult = std::unexpected{"Failed to fetch Clio nodes data"};
    }

    if (!expectedResult.has_value()) {
        return std::unexpected{std::move(expectedResult).error()};
    }

    std::vector<ClioNode> otherNodesData;
    for (auto const& [uuid, nodeDataStr] : expectedResult.value()) {
        if (uuid == *selfUuid_) {
            continue;
        }

        boost::system::error_code errorCode;
        auto const json = boost::json::parse(nodeDataStr, errorCode);
        if (errorCode.failed()) {
            return std::unexpected{fmt::format("Error parsing json from DB: {}", nodeDataStr)};
        }

        auto expectedNodeData = boost::json::try_value_to<ClioNode>(json);
        if (expectedNodeData.has_error()) {
            return std::unexpected{
                fmt::format("Error converting json to ClioNode: {}", nodeDataStr)
            };
        }
        *expectedNodeData->uuid = uuid;
        otherNodesData.push_back(std::move(expectedNodeData).value());
    }
    otherNodesData.push_back(ClioNode::from(selfUuid_, *writerState_, *cacheLoadingState_));
    return otherNodesData;
}

void
Backend::doWrite()
{
    auto const selfData = ClioNode::from(selfUuid_, *writerState_, *cacheLoadingState_);
    boost::json::value jsonValue{};
    boost::json::value_from(selfData, jsonValue);
    backend_->writeNodeMessage(*selfData.uuid, boost::json::serialize(jsonValue.as_object()));
}

}  // namespace cluster
