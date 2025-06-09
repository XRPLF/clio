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

#include "etlng/impl/TaskManager.hpp"

#include "etlng/ExtractorInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/MonitorInterface.hpp"
#include "etlng/SchedulerInterface.hpp"
#include "etlng/impl/Monitor.hpp"
#include "etlng/impl/TaskQueue.hpp"
#include "util/LedgerUtils.hpp"
#include "util/Profiler.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/log/Logger.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ranges>
#include <thread>
#include <utility>
#include <vector>

namespace etlng::impl {

TaskManager::TaskManager(
    util::async::AnyExecutionContext ctx,
    std::shared_ptr<SchedulerInterface> scheduler,
    std::reference_wrapper<ExtractorInterface> extractor,
    std::reference_wrapper<LoaderInterface> loader,
    std::reference_wrapper<MonitorInterface> monitor,
    uint32_t startSeq
)
    : ctx_(std::move(ctx))
    , schedulers_(std::move(scheduler))
    , extractor_(extractor)
    , loader_(loader)
    , monitor_(monitor)
    , queue_({.startSeq = startSeq, .increment = 1u, .limit = kQUEUE_SIZE_LIMIT})
{
}

TaskManager::~TaskManager()
{
    stop();
}

void
TaskManager::run(std::size_t numExtractors)
{
    LOG(log_.debug()) << "Starting task manager with " << numExtractors << " extractors...\n";

    stop();
    extractors_.clear();
    loaders_.clear();

    extractors_.reserve(numExtractors);
    for ([[maybe_unused]] auto _ : std::views::iota(0uz, numExtractors))
        extractors_.push_back(spawnExtractor(queue_));

    // Only one forward loader for now. Backfill to be added here later
    loaders_.push_back(spawnLoader(queue_));
}

util::async::AnyOperation<void>
TaskManager::spawnExtractor(TaskQueue& queue)
{
    // TODO: these values may be extracted to config later and/or need to be fine-tuned on a realistic system
    static constexpr auto kDELAY_BETWEEN_ATTEMPTS = std::chrono::milliseconds{100u};
    static constexpr auto kDELAY_BETWEEN_ENQUEUE_ATTEMPTS = std::chrono::milliseconds{1u};

    return ctx_.execute([this, &queue](auto stopRequested) {
        while (not stopRequested) {
            if (auto task = schedulers_->next(); task.has_value()) {
                if (auto maybeBatch = extractor_.get().extractLedgerWithDiff(task->seq); maybeBatch.has_value()) {
                    LOG(log_.debug()) << "Adding data after extracting diff";
                    while (not queue.enqueue(*maybeBatch)) {
                        // TODO (https://github.com/XRPLF/clio/issues/1852)
                        std::this_thread::sleep_for(kDELAY_BETWEEN_ENQUEUE_ATTEMPTS);

                        if (stopRequested)
                            break;
                    }
                } else {
                    // TODO: how do we signal to the loaders that it's time to shutdown? some special task?
                    break;  // TODO: handle server shutdown or other node took over ETL
                }
            } else {
                // TODO (https://github.com/XRPLF/clio/issues/1852)
                std::this_thread::sleep_for(kDELAY_BETWEEN_ATTEMPTS);
            }
        }
    });
}

util::async::AnyOperation<void>
TaskManager::spawnLoader(TaskQueue& queue)
{
    static constexpr auto kNANO_TO_SECOND = 1.0e9;

    return ctx_.execute([this, &queue](auto stopRequested) {
        while (not stopRequested) {
            // TODO (https://github.com/XRPLF/clio/issues/66): does not tell the loader whether it's out of order or not
            if (auto data = queue.dequeue(); data.has_value()) {
                auto nanos = util::timed<std::chrono::nanoseconds>([this, data = *data] { loader_.get().load(data); });
                auto const seconds = nanos / kNANO_TO_SECOND;
                auto const txnCount = data->transactions.size();
                auto const objCount = data->objects.size();

                LOG(log_.info()) << "Wrote ledger " << data->seq << " with header: " << util::toString(data->header)
                                 << ". txns[" << txnCount << "]; objs[" << objCount << "]; in " << seconds
                                 << " seconds;"
                                 << " tps[" << txnCount / seconds << "], ops[" << objCount / seconds << "]";

                monitor_.get().notifyLedgerLoaded(data->seq);
            }
        }
    });
}

void
TaskManager::wait()
{
    for (auto& extractor : extractors_)
        extractor.wait();
    for (auto& loader : loaders_)
        loader.wait();
}

void
TaskManager::stop()
{
    for (auto& extractor : extractors_)
        extractor.abort();
    for (auto& loader : loaders_)
        loader.abort();

    wait();
}

}  // namespace etlng::impl
