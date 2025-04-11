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

#include "cluster/ClusterCommunicationService.hpp"

#include "cluster/ClioNode.hpp"
#include "data/BackendInterface.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <ctime>
#include <memory>
#include <utility>
#include <vector>

namespace cluster {

ClusterCommunicationService::ClusterCommunicationService(
    std::shared_ptr<data::BackendInterface> backend,
    std::chrono::steady_clock::duration readInterval,
    std::chrono::steady_clock::duration writeInterval
)
    : backend_(std::move(backend))
    , readInterval_(readInterval)
    , writeInterval_(writeInterval)
    , selfData_{ClioNode{
          .uuid = std::make_shared<boost::uuids::uuid>(boost::uuids::random_generator{}()),
          .updateTime = std::chrono::system_clock::time_point{}
      }}
{
    nodesInClusterMetric_.set(1);  // The node always sees itself
    isHealthy_ = true;
}

void
ClusterCommunicationService::run()
{
    boost::asio::spawn(strand_, [this](boost::asio::yield_context yield) {
        boost::asio::steady_timer timer(yield.get_executor());
        while (true) {
            timer.expires_after(readInterval_);
            timer.async_wait(yield);
            doRead(yield);
        }
    });

    boost::asio::spawn(strand_, [this](boost::asio::yield_context yield) {
        boost::asio::steady_timer timer(yield.get_executor());
        while (true) {
            doWrite();
            timer.expires_after(writeInterval_);
            timer.async_wait(yield);
        }
    });
}

ClusterCommunicationService::~ClusterCommunicationService()
{
    stop();
}

void
ClusterCommunicationService::stop()
{
    if (stopped_)
        return;

    ctx_.stop();
    ctx_.join();
    stopped_ = true;
}

std::shared_ptr<boost::uuids::uuid>
ClusterCommunicationService::selfUuid() const
{
    // Uuid never changes so it is safe to copy it without using strand_
    return selfData_.uuid;
}

ClioNode
ClusterCommunicationService::selfData() const
{
    ClioNode result{};
    boost::asio::spawn(strand_, [this, &result](boost::asio::yield_context) { result = selfData_; });
    return result;
}

std::vector<ClioNode>
ClusterCommunicationService::clusterData() const
{
    std::vector<ClioNode> result;
    boost::asio::spawn(strand_, [this, &result](boost::asio::yield_context) {
        result = otherNodesData_;
        result.push_back(selfData_);
    });
    return result;
}

void
ClusterCommunicationService::doRead(boost::asio::yield_context yield)
{
    otherNodesData_.clear();

    auto const expectedResult = backend_->fetchClioNodesData(yield);
    if (!expectedResult.has_value()) {
        LOG(log_.error()) << "Failed to fetch nodes data";
        isHealthy_ = false;
        return;
    }

    // Create a new vector here to not have partially parsed data in otherNodesData_
    std::vector<ClioNode> otherNodesData;
    for (auto const& [uuid, nodeDataStr] : expectedResult.value()) {
        if (uuid == *selfData_.uuid) {
            continue;
        }

        boost::system::error_code errorCode;
        auto const json = boost::json::parse(nodeDataStr, errorCode);
        if (errorCode.failed()) {
            LOG(log_.error()) << "Error parsing json from DB: " << nodeDataStr;
            isHealthy_ = false;
            return;
        }

        auto expectedNodeData = boost::json::try_value_to<ClioNode>(json);
        if (expectedNodeData.has_error()) {
            LOG(log_.error()) << "Error converting json to ClioNode: " << json;
            isHealthy_ = false;
            return;
        }
        *expectedNodeData->uuid = uuid;
        otherNodesData.push_back(std::move(expectedNodeData).value());
    }
    otherNodesData_ = std::move(otherNodesData);
    nodesInClusterMetric_.set(otherNodesData_.size() + 1);
    isHealthy_ = true;
}

void
ClusterCommunicationService::doWrite()
{
    selfData_.updateTime = std::chrono::system_clock::now();
    boost::json::value jsonValue{};
    boost::json::value_from(selfData_, jsonValue);
    backend_->writeNodeMessage(*selfData_.uuid, boost::json::serialize(jsonValue.as_object()));
}

}  // namespace cluster
