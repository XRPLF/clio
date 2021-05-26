#include<reporting/server/SubscriptionManager.h>
#include<handlers/RPCHelpers.h>
    
void
SubscriptionManager::subLedger(std::shared_ptr<session>& session)
{
    streamSubscribers_[Ledgers].emplace(std::move(session));
}

void
SubscriptionManager::unsubLedger(std::shared_ptr<session>& session)
{
    streamSubscribers_[Ledgers].erase(session);
}

void
SubscriptionManager::pubLedger(
    ripple::LedgerInfo const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t txnCount)
{
    boost::json::object pubMsg;

    pubMsg["type"] = "ledgerClosed";
    pubMsg["ledger_index"] = lgrInfo.seq;
    pubMsg["ledger_hash"] = to_string(lgrInfo.hash);
    pubMsg["ledger_time"] = lgrInfo.closeTime.time_since_epoch().count();

    pubMsg["fee_ref"] = getJson(fees.units.jsonClipped());
    pubMsg["fee_base"] = getJson(fees.base.jsonClipped());
    pubMsg["reserve_base"] = getJson(fees.accountReserve(0).jsonClipped());
    pubMsg["reserve_inc"] = getJson(fees.increment.jsonClipped());

    pubMsg["validated_ledgers"] = ledgerRange;
    pubMsg["txn_count"] = txnCount;

    for (auto const& session: streamSubscribers_[Ledgers])
        session->send(boost::json::serialize(pubMsg));
}

void
SubscriptionManager::subTransactions(std::shared_ptr<session>& session)
{
    streamSubscribers_[Transactions].emplace(std::move(session));
}

void
SubscriptionManager::unsubTransactions(std::shared_ptr<session>& session)
{
    streamSubscribers_[Transactions].erase(session);
}

void
SubscriptionManager::subAccount(
    ripple::AccountID const& account, 
    std::shared_ptr<session>& session)
{
    accountSubscribers_[account].emplace(std::move(session));
}

void
SubscriptionManager::unsubAccount(
    ripple::AccountID const& account, 
    std::shared_ptr<session>& session)
{
    accountSubscribers_[account].erase(session);
}

void
SubscriptionManager::pubTransaction(
    Backend::TransactionAndMetadata const& blob,
    std::uint32_t seq)
{
    auto [tx, meta] = deserializeTxPlusMeta(blob, seq);

    boost::json::object pubMsg;
    pubMsg["transaction"] = getJson(*tx);
    pubMsg["meta"] = getJson(*meta);

    for (auto const& session: streamSubscribers_[Transactions])
        session->send(boost::json::serialize(pubMsg));

    auto journal = ripple::debugLog();
    auto accounts = meta->getAffectedAccounts(journal);

    for (ripple::AccountID const& account : accounts)
        for (auto const& session: accountSubscribers_[account])
            session->send(boost::json::serialize(pubMsg));
}