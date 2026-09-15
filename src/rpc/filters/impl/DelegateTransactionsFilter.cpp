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

std::optional<TransactionFilter::CheckResult>
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
        return std::nullopt;

    switch (delegateFilter_.delegateType) {
        case DelegateFilter::Role::Authorizer:
            // The queried account is the actor (signer) and the user wants to find the
            // authorizer (owner) it acted for.
            if (*txDelegate == queriedAccount_ && (!counterparty_ || *counterparty_ == txAccount))
                return CheckResult{txAccount, DelegateFilter::Role::Authorizer};
            break;

        case DelegateFilter::Role::Actor:
            // The queried account is the authorizer (owner) and the user wants to find the
            // actor (signer) that acted on its behalf.
            if (txAccount == queriedAccount_ && (!counterparty_ || *counterparty_ == *txDelegate))
                return CheckResult{*txDelegate, DelegateFilter::Role::Actor};
            break;
    }

    return std::nullopt;
}

}  // namespace rpc
