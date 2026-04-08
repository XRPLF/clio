#pragma once

#include "etl/Models.hpp"

#include <cstdint>
#include <optional>

namespace etl {

/**
 * @brief An interface for the Extractor
 */
struct ExtractorInterface {
    virtual ~ExtractorInterface() = default;

    /**
     * @brief Extract diff data for a particular ledger
     *
     * @param seq sequence of the ledger to extract
     * @return Ledger data diff between sequence and parent if available
     */
    [[nodiscard]] virtual std::optional<model::LedgerData>
    extractLedgerWithDiff(uint32_t seq) = 0;

    /**
     * @brief Extract data for a particular ledger
     *
     * @param seq sequence of the ledger to extract
     * @return Ledger header and transaction+metadata blobs if available
     */
    [[nodiscard]] virtual std::optional<model::LedgerData>
    extractLedgerOnly(uint32_t seq) = 0;
};

}  // namespace etl
