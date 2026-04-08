#pragma once

#include "data/BackendInterface.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/MonitorProviderInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/impl/Monitor.hpp"
#include "util/async/AnyExecutionContext.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>

namespace etl::impl {

class MonitorProvider : public MonitorProviderInterface {
public:
    std::unique_ptr<MonitorInterface>
    make(
        util::async::AnyExecutionContext ctx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
        uint32_t startSequence,
        std::chrono::steady_clock::duration dbStalledReportDelay
    ) override
    {
        return std::make_unique<Monitor>(
            std::move(ctx),
            std::move(backend),
            std::move(validatedLedgers),
            startSequence,
            dbStalledReportDelay
        );
    }
};

}  // namespace etl::impl
