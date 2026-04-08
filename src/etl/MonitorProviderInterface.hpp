#pragma once

#include "data/BackendInterface.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "util/async/AnyExecutionContext.hpp"

#include <chrono>
#include <cstdint>
#include <memory>

namespace etl {

/**
 * @brief An interface for providing Monitor instances
 */
struct MonitorProviderInterface {
    /**
     * @brief The time Monitor should wait before reporting absence of updates to the database
     */
    static constexpr auto kDEFAULT_DB_STALLED_REPORT_DELAY = std::chrono::seconds{10};

    virtual ~MonitorProviderInterface() = default;

    /**
     * @brief Create a new Monitor instance
     *
     * @param ctx The execution context for asynchronous operations
     * @param backend Interface to the backend database
     * @param validatedLedgers Interface for accessing network validated ledgers
     * @param startSequence The sequence number to start monitoring from
     * @param dbStalledReportDelay The timeout duration after which to signal no database updates
     * @return A unique pointer to a Monitor implementation
     */
    [[nodiscard]] virtual std::unique_ptr<MonitorInterface>
    make(
        util::async::AnyExecutionContext ctx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
        uint32_t startSequence,
        std::chrono::steady_clock::duration dbStalledReportDelay = kDEFAULT_DB_STALLED_REPORT_DELAY
    ) = 0;
};

}  // namespace etl
