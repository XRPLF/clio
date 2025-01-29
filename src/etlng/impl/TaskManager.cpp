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
#include "etlng/SchedulerInterface.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/AnyStrand.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/protocol/TxFormats.h>

#include <chrono>
#include <cstddef>
#include <functional>
#include <ranges>
#include <thread>
#include <utility>
#include <vector>

namespace etlng::impl {

TaskManager::TaskManager(
    util::async::AnyExecutionContext&& ctx,
    std::reference_wrapper<SchedulerInterface> scheduler,
    std::reference_wrapper<ExtractorInterface> extractor,
    std::reference_wrapper<LoaderInterface> loader
)
    : ctx_(std::move(ctx)), schedulers_(scheduler), extractor_(extractor), loader_(loader)
{
}

TaskManager::~TaskManager()
{
    stop();
}

void
TaskManager::run(Settings settings)
{
    static constexpr auto kQUEUE_SIZE_LIMIT = 2048uz;

    auto schedulingStrand = ctx_.makeStrand();
    PriorityQueue queue(ctx_.makeStrand(), kQUEUE_SIZE_LIMIT);

    LOG(log_.debug()) << "Starting task manager...\n";

    extractors_.reserve(settings.numExtractors);
    for ([[maybe_unused]] auto _ : std::views::iota(0uz, settings.numExtractors))
        extractors_.push_back(spawnExtractor(schedulingStrand, queue));

    loaders_.reserve(settings.numLoaders);
    for ([[maybe_unused]] auto _ : std::views::iota(0uz, settings.numLoaders))
        loaders_.push_back(spawnLoader(queue));

    wait();
    LOG(log_.debug()) << "All finished in task manager..\n";
}

util::async::AnyOperation<void>
TaskManager::spawnExtractor(util::async::AnyStrand& strand, PriorityQueue& queue)
{
    // TODO: these values may be extracted to config later and/or need to be fine-tuned on a realistic system
    static constexpr auto kDELAY_BETWEEN_ATTEMPTS = std::chrono::milliseconds{100u};
    static constexpr auto kDELAY_BETWEEN_ENQUEUE_ATTEMPTS = std::chrono::milliseconds{1u};

    return strand.execute([this, &queue](auto stopRequested) {
        while (not stopRequested) {
            if (auto task = schedulers_.get().next(); task.has_value()) {
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
TaskManager::spawnLoader(PriorityQueue& queue)
{
    return ctx_.execute([this, &queue](auto stopRequested) {
        while (not stopRequested) {
            // TODO (https://github.com/XRPLF/clio/issues/66): does not tell the loader whether it's out of order or not
            if (auto data = queue.dequeue(); data.has_value())
                loader_.get().load(*data);
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
