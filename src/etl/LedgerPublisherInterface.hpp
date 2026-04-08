#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace etl {

/**
 * @brief The interface of a scheduler for the extraction process
 */
struct LedgerPublisherInterface {
    virtual ~LedgerPublisherInterface() = default;

    /**
     * @brief Publish the ledger by its sequence number
     *
     * @param seq The sequence number of the ledger
     * @param maxAttempts The maximum number of attempts to publish the ledger; no limit if nullopt
     * @param attemptsDelay The delay between attempts
     * @return Whether the ledger was found in the database and published
     */
    virtual bool
    publish(
        uint32_t seq,
        std::optional<uint32_t> maxAttempts,
        std::chrono::steady_clock::duration attemptsDelay = std::chrono::seconds{1}
    ) = 0;

    /**
     * @brief Get last publish time as a time point
     *
     * @return A std::chrono::time_point representing the time of the last publish
     */
    virtual std::chrono::time_point<std::chrono::system_clock>
    getLastPublish() const = 0;

    /**
     * @brief Get time passed since last ledger close, in seconds
     *
     * @return The number of seconds since the last ledger close as std::uint32_t
     */
    virtual std::uint32_t
    lastCloseAgeSeconds() const = 0;

    /**
     * @brief Get time passed since last publish, in seconds
     *
     * @return The number of seconds since the last publish as std::uint32_t
     */
    virtual std::uint32_t
    lastPublishAgeSeconds() const = 0;
};

}  // namespace etl
