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
SubscriptionManager::sendAll(
    std::string const& pubMsg,
    std::unordered_set<std::shared_ptr<WsBase>>& subs)
{
    std::lock_guard lk(m_);
    for (auto it = subs.begin(); it != subs.end();)
    {
        auto& session = *it;
        if (session->dead())
        {
            it = subs.erase(it);
        }
        else
        {
            session->publishToStream(pubMsg);
            ++it;
        }
    }
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

    sendAll(boost::json::serialize(pubMsg), streamSubscribers_[Ledgers]);
}

void
SubscriptionManager::pubTransaction(
    Backend::TransactionAndMetadata const& blobs,
    std::uint32_t seq)
{
    auto [tx, meta] = RPC::deserializeTxPlusMeta(blobs, seq);
    boost::json::object pubObj;
    pubObj["transaction"] = RPC::toJson(*tx);
    pubObj["meta"] = RPC::toJson(*meta);
    std::string pubMsg{boost::json::serialize(pubObj)};
    sendAll(pubMsg, streamSubscribers_[Transactions]);

    auto journal = ripple::debugLog();
    auto accounts = meta->getAffectedAccounts(journal);

    for (ripple::AccountID const& account : accounts)
        sendAll(pubMsg, accountSubscribers_[account]);

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
                    sendAll(pubMsg, bookSubscribers_[book]);
                }
            }
        }
    }
}

void
SubscriptionManager::forwardProposedTransaction(
    boost::json::object const& response)
{
    std::string pubMsg{boost::json::serialize(pubMsg)};
    sendAll(pubMsg, streamSubscribers_[TransactionsProposed]);

    auto transaction = response.at("transaction").as_object();
    auto accounts = RPC::getAccountsFromTransaction(transaction);

    for (ripple::AccountID const& account : accounts)
        sendAll(pubMsg, accountProposedSubscribers_[account]);
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

