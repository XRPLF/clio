#pragma once

#include "data/Types.hpp"

#include <xrpl/protocol/AccountID.h>

#include <optional>

namespace rpc {

/**
 * @brief Interface for filtering transactions.
 */
class TransactionFilter {
public:
    virtual ~TransactionFilter() = default;

    /**
     * @brief Check if a transaction blob matches the filter criteria.
     * @param txnPlusMeta The transaction and metadata blob from the backend.
     * @return The relevant account to report for this txn if it should be included in the output
     * Json, or std::nullopt if the txn should be excluded
     */
    [[nodiscard]] virtual std::optional<xrpl::AccountID>
    check(data::TransactionAndMetadata const& txnPlusMeta) const = 0;
};

}  // namespace rpc
