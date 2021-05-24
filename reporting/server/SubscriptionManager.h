//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef SUBSCRIPTION_MANAGER_H
#define SUBSCRIPTION_MANAGER_H

#include <reporting/server/session.h>

#include <set>
#include <memory>

class session;

class SubscriptionManager
{
    using subscriptions = std::set<std::shared_ptr<session>>;

    enum SubscriptionType {
        Ledgers,
        Transactions,
        
        finalEntry
    };

    std::array<subscriptions, finalEntry> subscribers_;
    std::unordered_map<ripple::AccountID, subscriptions> accountSubscribers_;

public:
    void
    subLedger(std::shared_ptr<session>& session);

    void
    pubLedger(
        ripple::LedgerInfo const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount);

    void
    unsubLedger(std::shared_ptr<session>& session);

    void
    subTransactions(std::shared_ptr<session>& session);

    void
    unsubTransactions(std::shared_ptr<session>& session);

    void
    pubTransaction(Backend::TransactionAndMetadata const& blob);

    void
    subAccount(ripple::AccountID const& account, std::shared_ptr<session>& session);

    void
    unsubAccount(ripple::AccountID const& account, std::shared_ptr<session>& session);

};

#endif //SUBSCRIPTION_MANAGER_H