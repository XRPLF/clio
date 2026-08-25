#pragma once

#include "data/Types.hpp"
#include "rpc/common/Types.hpp"

#include <xrpl/protocol/AccountID.h>

#include <optional>

namespace rpc {

/**
 * @brief Interface for filtering transactions.
 */
class TransactionFilter {
public:
    /**
     * @brief The result of a filter check that matched a transaction.
     */
    struct CheckResult {
        /**
         * @brief Construct a check result
         *
         * @param account The relevant account to report for this txn
         * @param role The role `account` played in this txn
         */
        CheckResult(xrpl::AccountID account, DelegateFilter::Role role)
            : account{account}, role{role}
        {
        }

        xrpl::AccountID account;
        DelegateFilter::Role role;
    };

    virtual ~TransactionFilter() = default;

    /**
     * @brief Check if a transaction blob matches the filter criteria.
     *
     * @param txnPlusMeta The transaction and metadata blob from the backend.
     * @return The relevant account and role to report for this txn if it should be included in
     * the output Json, or std::nullopt if the txn should be excluded
     */
    [[nodiscard]] virtual std::optional<CheckResult>
    check(data::TransactionAndMetadata const& txnPlusMeta) const = 0;
};

}  // namespace rpc
