//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

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
public:
    /**
     * @brief Construct a new delegate transaction filter
     * @param filter The filter parameters from the JSON request (role, counterparty string)
     * @param queriedAccount The account currently being queried in account_tx (input from
     * account_tx handler)
     */
    DelegateTransactionFilter(rpc::DelegateFilter filter, xrpl::AccountID queriedAccount);

    [[nodiscard]] FilterResult
    check(data::TransactionAndMetadata const& txnPlusMeta) const override;

private:
    rpc::DelegateFilter delegateFilter_;
    xrpl::AccountID queriedAccount_;
    std::optional<xrpl::AccountID> counterparty_;
};

}  // namespace rpc
