#pragma once

#include "etl/ExtractorInterface.hpp"
#include "etl/LoaderInterface.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/TaskManagerInterface.hpp"
#include "etl/TaskManagerProviderInterface.hpp"
#include "etl/impl/Scheduling.hpp"
#include "etl/impl/TaskManager.hpp"
#include "util/async/AnyExecutionContext.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace etl::impl {

/**
 * @brief Implementation of the TaskManagerProvider interface
 */
class TaskManagerProvider : public TaskManagerProviderInterface {
    std::reference_wrapper<NetworkValidatedLedgersInterface> ledgers_;
    std::shared_ptr<ExtractorInterface> extractor_;
    std::shared_ptr<LoaderInterface> loader_;

public:
    /**
     * @brief Constructor
     *
     * @param ledgers Reference to ledgers
     * @param extractor The extractor
     * @param loader The loader
     */
    TaskManagerProvider(
        std::reference_wrapper<NetworkValidatedLedgersInterface> ledgers,
        std::shared_ptr<ExtractorInterface> extractor,
        std::shared_ptr<LoaderInterface> loader
    )
        : ledgers_(ledgers), extractor_(std::move(extractor)), loader_(std::move(loader))
    {
    }

    std::unique_ptr<TaskManagerInterface>
    make(
        util::async::AnyExecutionContext ctx,
        std::reference_wrapper<MonitorInterface> monitor,
        uint32_t startSeq,
        std::optional<uint32_t> finishSeq
    ) override
    {
        auto scheduler = impl::makeScheduler(impl::ForwardScheduler{ledgers_, startSeq, finishSeq});
        // TODO: add impl::BackfillScheduler{startSeq - 1, startSeq - ...},

        return std::make_unique<TaskManager>(
            std::move(ctx), std::move(scheduler), *extractor_, *loader_, monitor, startSeq
        );
    }
};

}  // namespace etl::impl
