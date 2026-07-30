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

    // The account is always the owner whose funds move; when sfDelegate is present, sfAccount is
    // the "authorizer" and sfDelegate is the "actor" that signed on its behalf.
    auto const txAccount = sttx.getAccountID(xrpl::sfAccount);

    std::optional<xrpl::AccountID> txDelegate;
    if (sttx.isFieldPresent(xrpl::sfDelegate))
        txDelegate = sttx.getAccountID(xrpl::sfDelegate);

    // Transactions without an sfDelegate field are not delegated; exclude them immediately.
    if (not txDelegate.has_value())
        return {.shouldInclude = false, .relevantAccount = std::nullopt};

    // Filter by "authorizer" ie. the queried account is the actor (signer) and the user wants to
    // find the authorizer (owner) it acted for.
    if (delegateFilter_.delegateType == rpc::DelegateFilter::Role::Authorizer) {
        if (*txDelegate == queriedAccount_) {
            if (!counterparty_ || *counterparty_ == txAccount)
                return {.shouldInclude = true, .relevantAccount = txAccount};
        }
    }

    // Filter by "actor" ie. the queried account is the authorizer (owner) and the user wants to
    // find the actor (signer) that acted on its behalf.
    else if (delegateFilter_.delegateType == rpc::DelegateFilter::Role::Actor) {
        if (txAccount == queriedAccount_) {
            if (!counterparty_ || *counterparty_ == *txDelegate)
                return {.shouldInclude = true, .relevantAccount = txDelegate};
        }
    }

    return {.shouldInclude = false, .relevantAccount = std::nullopt};
}

}  // namespace rpc
