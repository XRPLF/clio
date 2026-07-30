#pragma once

#include "data/Types.hpp"

#include <xrpl/protocol/AccountID.h>

#include <optional>

namespace rpc {

/**
 * @brief Result of a filter check.
 */
struct FilterResult {
    bool shouldInclude = false;
    std::optional<xrpl::AccountID> relevantAccount;
};

/**
 * @brief Interface for filtering transactions.
 */
class TransactionFilter {
public:
    virtual ~TransactionFilter() = default;

    /**
     * @brief Check if a transaction blob matches the filter criteria.
     * @param txnPlusMeta The transaction and metadata blob from the backend.
     * @return FilterResult indicating if the txn should be included in the output Json or not
     */
    [[nodiscard]] virtual FilterResult
    check(data::TransactionAndMetadata const& txnPlusMeta) const = 0;
};

}  // namespace rpc
