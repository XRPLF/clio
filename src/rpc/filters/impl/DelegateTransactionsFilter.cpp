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

#include "rpc/filters/impl/DelegateTransactionsFilter.hpp"

#include "data/Types.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/filters/TransactionFilter.hpp"

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>

#include <optional>
#include <utility>

namespace rpc {

DelegateTransactionFilter::DelegateTransactionFilter(
    rpc::DelegateFilter filter,
    xrpl::AccountID queriedAccount
)
    : delegateFilter_(std::move(filter)), queriedAccount_(queriedAccount)
{
    if (delegateFilter_.counterParty)
        counterparty_ = xrpl::parseBase58<xrpl::AccountID>(*delegateFilter_.counterParty);
}

FilterResult
DelegateTransactionFilter::check(data::TransactionAndMetadata const& txnPlusMeta) const
{
    xrpl::SerialIter sit{txnPlusMeta.transaction.data(), txnPlusMeta.transaction.size()};
    xrpl::STTx const sttx{sit};

    // The account where the funds are withdrawn is always delegator
    auto const txAccount = sttx.getAccountID(xrpl::sfAccount);

    std::optional<xrpl::AccountID> txDelegate;
    if (sttx.isFieldPresent(xrpl::sfDelegate))
        txDelegate = sttx.getAccountID(xrpl::sfDelegate);

    // txn with no delegate filter should return immediately
    // Note: should already have been checked in handler code before calling this function though
    if (not txDelegate.has_value())
        return {.shouldInclude = false, .relevantAccount = std::nullopt};

    // Filter by "Delegator" ie. User wants to find the Owner.
    // This implies the user must be the Delegatee that acted on someone's behalf.
    if (delegateFilter_.delegateType == rpc::DelegateFilter::Role::Authorizer) {
        if (*txDelegate == queriedAccount_) {
            if (!counterparty_ || *counterparty_ == txAccount)
                return {.shouldInclude = true, .relevantAccount = txAccount};
        }
    }

    // Filter by "Delegatee" ie. User wants to find the Signer who acted on behalf of the user.
    // This implies the user must be the delegator.
    else if (delegateFilter_.delegateType == rpc::DelegateFilter::Role::Actor) {
        if (txAccount == queriedAccount_) {
            if (!counterparty_ || *counterparty_ == *txDelegate)
                return {.shouldInclude = true, .relevantAccount = txDelegate};
        }
    }

    return {.shouldInclude = false, .relevantAccount = std::nullopt};
}

}  // namespace rpc
