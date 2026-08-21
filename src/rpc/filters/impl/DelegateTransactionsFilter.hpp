#pragma once

#include "data/Types.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/filters/TransactionFilter.hpp"

#include <xrpl/protocol/AccountID.h>

#include <optional>

namespace rpc {

/**
 * @brief Delegate transaction filter to filter txn based on permission delegate
 */
class DelegateTransactionFilter : public TransactionFilter {
    rpc::DelegateFilter delegateFilter_;
    xrpl::AccountID queriedAccount_;
    std::optional<xrpl::AccountID> counterparty_;

public:
    /**
     * @brief Construct a new delegate transaction filter
     *
     * @param filter The filter parameters from the JSON request (role, counterparty string)
     * @param queriedAccount The account currently being queried in account_tx (input from
     * account_tx handler)
     */
    DelegateTransactionFilter(rpc::DelegateFilter filter, xrpl::AccountID queriedAccount);

    [[nodiscard]] std::optional<TransactionFilter::CheckResult>
    check(data::TransactionAndMetadata const& txnPlusMeta) const override;
};

}  // namespace rpc
