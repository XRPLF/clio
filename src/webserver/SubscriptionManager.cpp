#include <rpc/RPCHelpers.h>
#include <webserver/SubscriptionManager.h>
#include <webserver/WsBase.h>

void
SubscriptionManager::subLedger(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[Ledgers].emplace(session);
}

void
SubscriptionManager::unsubLedger(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
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

    pubMsg["fee_ref"] = RPC::toBoostJson(fees.units.jsonClipped());
    pubMsg["fee_base"] = RPC::toBoostJson(fees.base.jsonClipped());
    pubMsg["reserve_base"] =
        RPC::toBoostJson(fees.accountReserve(0).jsonClipped());
    pubMsg["reserve_inc"] = RPC::toBoostJson(fees.increment.jsonClipped());

    pubMsg["validated_ledgers"] = ledgerRange;
    pubMsg["txn_count"] = txnCount;

    std::lock_guard lk(m_);
    for (auto const& session : streamSubscribers_[Ledgers])
        session->send(boost::json::serialize(pubMsg));
}

void
SubscriptionManager::subTransactions(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[Transactions].emplace(session);
}

void
SubscriptionManager::unsubTransactions(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[Transactions].erase(session);
}

void
SubscriptionManager::subAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    accountSubscribers_[account].emplace(session);
}

void
SubscriptionManager::unsubAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    accountSubscribers_[account].erase(session);
}

void
SubscriptionManager::subBook(
    ripple::Book const& book,
    std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    bookSubscribers_[book].emplace(session);
}

void
SubscriptionManager::unsubBook(
    ripple::Book const& book,
    std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    bookSubscribers_[book].erase(session);
}

void
SubscriptionManager::pubTransaction(
    Backend::TransactionAndMetadata const& blobs,
    std::uint32_t seq)
{
    std::lock_guard lk(m_);

    auto [tx, meta] = RPC::deserializeTxPlusMeta(blobs, seq);
    boost::json::object pubMsg;
    pubMsg["transaction"] = RPC::toJson(*tx);
    pubMsg["meta"] = RPC::toJson(*meta);

    for (auto const& session : streamSubscribers_[Transactions])
        session->send(boost::json::serialize(pubMsg));

    auto journal = ripple::debugLog();
    auto accounts = meta->getAffectedAccounts(journal);

    for (ripple::AccountID const& account : accounts)
        for (auto const& session : accountSubscribers_[account])
            session->send(boost::json::serialize(pubMsg));

    for (auto const& node : meta->peekNodes())
    {
        if (!node.isFieldPresent(ripple::sfLedgerEntryType))
            assert(false);
        if (node.getFieldU16(ripple::sfLedgerEntryType) == ripple::ltOFFER)
        {
            ripple::SField const* field = nullptr;

            // We need a field that contains the TakerGets and TakerPays
            // parameters.
            if (node.getFName() == ripple::sfModifiedNode)
                field = &ripple::sfPreviousFields;
            else if (node.getFName() == ripple::sfCreatedNode)
                field = &ripple::sfNewFields;
            else if (node.getFName() == ripple::sfDeletedNode)
                field = &ripple::sfFinalFields;

            if (field)
            {
                auto data = dynamic_cast<const ripple::STObject*>(
                    node.peekAtPField(*field));

                if (data && data->isFieldPresent(ripple::sfTakerPays) &&
                    data->isFieldPresent(ripple::sfTakerGets))
                {
                    // determine the OrderBook
                    ripple::Book book{
                        data->getFieldAmount(ripple::sfTakerGets).issue(),
                        data->getFieldAmount(ripple::sfTakerPays).issue()};
                    for (auto const& session : bookSubscribers_[book])
                        session->send(boost::json::serialize(pubMsg));
                }
            }
        }
    }
}

void
SubscriptionManager::forwardProposedTransaction(
    boost::json::object const& response)
{
    std::lock_guard lk(m_);
    for (auto const& session : streamSubscribers_[TransactionsProposed])
        session->send(boost::json::serialize(response));

    auto transaction = response.at("transaction").as_object();
    auto accounts = RPC::getAccountsFromTransaction(transaction);

    for (ripple::AccountID const& account : accounts)
        for (auto const& session : accountProposedSubscribers_[account])
            session->send(boost::json::serialize(response));
}

void
SubscriptionManager::subProposedAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    accountProposedSubscribers_[account].emplace(session);
}

void
SubscriptionManager::unsubProposedAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    accountProposedSubscribers_[account].erase(session);
}

void
SubscriptionManager::subProposedTransactions(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[TransactionsProposed].emplace(session);
}

void
SubscriptionManager::unsubProposedTransactions(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[TransactionsProposed].erase(session);
}

void
SubscriptionManager::clearSession(WsBase* s)
{
    std::lock_guard lk(m_);

    // need the == operator. No-op delete
    std::shared_ptr<WsBase> targetSession(s, [](WsBase*) {});
    for (auto& stream : streamSubscribers_)
        stream.erase(targetSession);

    for (auto& [account, subscribers] : accountSubscribers_)
    {
        if (subscribers.find(targetSession) != subscribers.end())
            accountSubscribers_[account].erase(targetSession);
    }

    for (auto& [account, subscribers] : accountProposedSubscribers_)
    {
        if (subscribers.find(targetSession) != subscribers.end())
            accountProposedSubscribers_[account].erase(targetSession);
    }
}
