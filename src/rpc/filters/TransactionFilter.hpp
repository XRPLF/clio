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

#include <xrpl/protocol/AccountID.h>

#include <optional>

namespace rpc {

/**
 * @brief Result of a filter check.
 */
struct FilterResult {
    bool shouldInclude;
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
