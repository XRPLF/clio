#pragma once

#include "etl/Models.hpp"

#include <xrpl/protocol/LedgerHeader.h>

#include <expected>
#include <optional>

namespace etl {

/**
 * @brief Enumeration of possible errors that can occur during loading operations
 */
enum class LoaderError {
    AmendmentBlocked, /*< Error indicating that an operation is blocked by an amendment */
    WriteConflict,    /*< Error indicating that a write operation resulted in a conflict */
};

/**
 * @brief An interface for a ETL Loader
 */
struct LoaderInterface {
    virtual ~LoaderInterface() = default;

    /**
     * @brief Load ledger data
     * @param data The data to load
     * @return Nothing or error as std::expected
     */
    [[nodiscard]] virtual std::expected<void, LoaderError>
    load(model::LedgerData const& data) = 0;

    /**
     * @brief Load the initial ledger
     * @param data The data to load
     * @return Optional ledger header
     */
    [[nodiscard]] virtual std::optional<xrpl::LedgerHeader>
    loadInitialLedger(model::LedgerData const& data) = 0;
};

}  // namespace etl
