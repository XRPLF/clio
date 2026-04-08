#pragma once

#include "etl/ETLState.hpp"

#include <boost/json/object.hpp>

#include <cstdint>
#include <optional>

namespace etl {

/**
 * @brief This is a base class for any ETL service implementations.
 * @note A ETL service is responsible for continuously extracting data from a p2p node, and writing
 * that data to the databases.
 */
struct ETLServiceInterface {
    virtual ~ETLServiceInterface() = default;

    /**
     * @brief Start all components to run ETL service.
     */
    virtual void
    run() = 0;

    /**
     * @brief Stop the ETL service.
     * @note This method blocks until the ETL service has stopped.
     */
    virtual void
    stop() = 0;

    /**
     * @brief Get state of ETL as a JSON object
     *
     * @return The state of ETL as a JSON object
     */
    [[nodiscard]] virtual boost::json::object
    getInfo() const = 0;

    /**
     * @brief Check for the amendment blocked state.
     *
     * @return true if currently amendment blocked; false otherwise
     */
    [[nodiscard]] virtual bool
    isAmendmentBlocked() const = 0;

    /**
     * @brief Check whether Clio detected DB corruptions.
     *
     * @return true if corruption of DB was detected and cache was stopped.
     */
    [[nodiscard]] virtual bool
    isCorruptionDetected() const = 0;

    /**
     * @brief Get the etl nodes' state
     * @return The etl nodes' state, nullopt if etl nodes are not connected
     */
    [[nodiscard]] virtual std::optional<ETLState>
    getETLState() const = 0;

    /**
     * @brief Get time passed since last ledger close, in seconds.
     *
     * @return Time passed since last ledger close
     */
    [[nodiscard]] virtual std::uint32_t
    lastCloseAgeSeconds() const = 0;
};

}  // namespace etl
