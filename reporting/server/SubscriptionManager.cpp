#include<reporting/server/SubscriptionManager.h>
#include<handlers/RPCHelpers.h>
    
void
SubscriptionManager::subLedger(std::shared_ptr<session>& session)
{
    subscribers_[Ledgers].emplace(std::move(session));
}

void
SubscriptionManager::unsubLedger(std::shared_ptr<session>& session)
{
    subscribers_[Ledgers].erase(session);
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

    for (auto const& session: subscribers_[Ledgers])
        session->send(boost::json::serialize(pubMsg));
}

void
SubscriptionManager::subTransactions(std::shared_ptr<session>& session)
{
    subscribers_[Transactions].emplace(std::move(session));
}

void
SubscriptionManager::unsubTransactions(std::shared_ptr<session>& session)
{
    subscribers_[Transactions].erase(session);
}

void
SubscriptionManager::pubTransaction(Backend::TransactionAndMetadata const& blob)
{
    auto [tx, meta] = deserializeTxPlusMeta(blob);

    boost::json::object pubMsg;
    pubMsg["meta"] = getJson(*tx);
    pubMsg["transaction"] = getJson(*meta);

    for (auto const& session: subscribers_[Transactions])
        session->send(boost::json::serialize(pubMsg));
}
