#pragma once

#include "etl/ExtractorInterface.hpp"
#include "etl/LoaderInterface.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/SchedulerInterface.hpp"
#include "etl/TaskManagerInterface.hpp"
#include "etl/impl/Monitor.hpp"
#include "etl/impl/TaskQueue.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/protocol/TxFormats.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace etl::impl {

class TaskManager : public TaskManagerInterface {
    static constexpr auto kQUEUE_SIZE_LIMIT = 2048uz;

    util::async::AnyExecutionContext ctx_;
    std::shared_ptr<SchedulerInterface> schedulers_;
    std::reference_wrapper<ExtractorInterface> extractor_;
    std::reference_wrapper<LoaderInterface> loader_;
    std::reference_wrapper<MonitorInterface> monitor_;

    impl::TaskQueue queue_;
    std::atomic_uint32_t nextForwardSequence_;

    std::vector<util::async::AnyOperation<void>> extractors_;
    std::vector<util::async::AnyOperation<void>> loaders_;

    std::atomic_bool running_ = false;
    util::Logger log_{"ETL"};

public:
    TaskManager(
        util::async::AnyExecutionContext ctx,
        std::shared_ptr<SchedulerInterface> scheduler,
        std::reference_wrapper<ExtractorInterface> extractor,
        std::reference_wrapper<LoaderInterface> loader,
        std::reference_wrapper<MonitorInterface> monitor,
        uint32_t startSeq
    );

    ~TaskManager() override;

    TaskManager(TaskManager const&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager&
    operator=(TaskManager const&) = delete;
    TaskManager&
    operator=(TaskManager&&) = delete;

    void
    run(std::size_t numExtractors) override;

    void
    stop() override;

private:
    void
    wait();

    [[nodiscard]] util::async::AnyOperation<void>
    spawnExtractor(TaskQueue& queue);

    [[nodiscard]] util::async::AnyOperation<void>
    spawnLoader(TaskQueue& queue);
};

}  // namespace etl::impl
