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

#include "data/BackendInterface.hpp"
#include "etl/WriterState.hpp"

#include <chrono>
#include <ctime>
#include <memory>
#include <utility>

namespace cluster {

ClusterCommunicationService::ClusterCommunicationService(
    std::shared_ptr<data::BackendInterface> backend,
    std::unique_ptr<etl::WriterStateInterface> writerState,
    std::chrono::steady_clock::duration readInterval,
    std::chrono::steady_clock::duration writeInterval
)
    : backend_(ctx_, std::move(backend), writerState->clone(), readInterval, writeInterval)
    , writerDecider_(ctx_, std::move(writerState))
{
}

void
ClusterCommunicationService::run()
{
    backend_.subscribeToNewState([this](auto&&... args) {
        metrics_.onNewState(std::forward<decltype(args)>(args)...);
    });
    backend_.subscribeToNewState([this](auto&&... args) {
        writerDecider_.onNewState(std::forward<decltype(args)>(args)...);
    });
    backend_.run();
}

ClusterCommunicationService::~ClusterCommunicationService()
{
    stop();
}

void
ClusterCommunicationService::stop()
{
    backend_.stop();
}

}  // namespace cluster
