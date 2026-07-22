#include "etl/impl/TaskManager.hpp"

#include "etl/ExtractorInterface.hpp"
#include "etl/LoaderInterface.hpp"
#include "etl/Models.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/SchedulerInterface.hpp"
#include "etl/impl/Monitor.hpp"
#include "etl/impl/TaskQueue.hpp"
#include "util/Assert.hpp"
#include "util/Constants.hpp"
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

namespace etl::impl {

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
    , queue_({.startSeq = startSeq, .increment = 1u, .limit = kQueueSizeLimit})
{
}

TaskManager::~TaskManager()
{
    stop();
}

void
TaskManager::run(std::size_t numExtractors)
{
    ASSERT(not running_, "TaskManager can only be started once");
    running_ = true;

    LOG(log_.debug()) << "Starting task manager with " << numExtractors << " extractors...\n";

    extractors_.reserve(numExtractors);
    for ([[maybe_unused]] auto _ : std::views::iota(0uz, numExtractors))
        extractors_.push_back(spawnExtractor(queue_));

    // Only one forward loader for now. Backfill to be added here later
    loaders_.push_back(spawnLoader(queue_));
}

util::async::AnyOperation<void>
TaskManager::spawnExtractor(TaskQueue& queue)
{
    // TODO https://github.com/XRPLF/clio/issues/2838: the approach should be changed to a reactive
    // one instead
    static constexpr auto kDelayBetweenAttempts = std::chrono::milliseconds{10u};
    static constexpr auto kDelayBetweenEnqueueAttempts = std::chrono::milliseconds{1u};

    return ctx_.execute([this, &queue](auto stopRequested) {
        while (not stopRequested) {
            if (auto task = schedulers_->next(); task.has_value()) {
                if (auto maybeBatch = extractor_.get().extractLedgerWithDiff(task->seq);
                    maybeBatch.has_value()) {
                    LOG(log_.debug()) << "Adding data after extracting diff";
                    while (not queue.enqueue(*maybeBatch)) {
                        // TODO (https://github.com/XRPLF/clio/issues/1852)
                        std::this_thread::sleep_for(kDelayBetweenEnqueueAttempts);

                        if (stopRequested)
                            break;
                    }
                }
            } else {
                // TODO (https://github.com/XRPLF/clio/issues/1852)
                std::this_thread::sleep_for(kDelayBetweenAttempts);
            }
        }

        LOG(log_.info()) << "Extractor (one of) coroutine stopped";
    });
}

util::async::AnyOperation<void>
TaskManager::spawnLoader(TaskQueue& queue)
{
    return ctx_.execute([this, &queue](auto stopRequested) {
        while (not stopRequested) {
            // TODO (https://github.com/XRPLF/clio/issues/66): does not tell the loader whether it's
            // out of order or not
            if (auto data = queue.dequeue(); data.has_value()) {
                auto [expectedSuccess, nanos] = util::timed<std::chrono::nanoseconds>([&] {
                    return loader_.get().load(*data);
                });

                auto const shouldExitOnError = [&] {
                    if (expectedSuccess.has_value())
                        return false;

                    switch (expectedSuccess.error()) {
                        case LoaderError::WriteConflict:
                            LOG(log_.warn()) << "Immediately stopping loader on write conflict"
                                             << "; latest ledger cache loaded for " << data->seq;
                            monitor_.get().notifyWriteConflict(data->seq);
                            return true;
                        case LoaderError::AmendmentBlocked:
                            LOG(log_.warn()) << "Immediately stopping loader on amendment block";
                            return true;
                    }

                    std::unreachable();
                }();

                if (shouldExitOnError)
                    break;

                auto const seconds = nanos / util::kNanoPerSecond;
                auto const txnCount = data->transactions.size();
                auto const objCount = data->objects.size();

                LOG(log_.info()) << "Wrote ledger " << data->seq
                                 << " with header: " << util::toString(data->header) << ". txns["
                                 << txnCount << "]; objs[" << objCount << "]; in " << seconds
                                 << " seconds;"
                                 << " tps[" << txnCount / seconds << "], ops[" << objCount / seconds
                                 << "]";

                monitor_.get().notifySequenceLoaded(data->seq);
            } else {
                // TODO (https://github.com/XRPLF/clio/issues/1852) this is probably better done
                // with a timeout (on coroutine) so that the thread itself is not blocked. for now
                // this implies that the context (io_threads) needs at least 2 threads
                queue.awaitTask();
            }
        }

        LOG(log_.info()) << "Loader coroutine stopped";
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
    ASSERT(running_, "TaskManager is not running");

    for (auto& extractor : extractors_)
        extractor.abort();
    for (auto& loader : loaders_)
        loader.abort();

    queue_.stop();
    wait();
}

}  // namespace etl::impl
