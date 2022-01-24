#include <rpc/RPCHelpers.h>
#include <subscriptions/SubscriptionManager.h>
#include <webserver/WsBase.h>

template <class T>
inline void
sendToSubscribers(
    std::string const& message,
    T& subscribers,
    boost::asio::io_context::strand& strand)
{
    boost::asio::post(strand, [&subscribers, message]() {
        for (auto it = subscribers.begin(); it != subscribers.end();)
        {
            auto& session = *it;
            if (session->dead())
            {
                it = subscribers.erase(it);
            }
            else
            {
                session->send(message);
                ++it;
            }
        }
    });
}

template <class T>
inline void
addSession(
    std::shared_ptr<WsBase> session,
    T& subscribers,
    boost::asio::io_context::strand& strand)
{
    boost::asio::post(strand, [&subscribers, s = std::move(session)]() {
        subscribers.emplace(s);
    });
}

template <class T>
inline void
removeSession(
    std::shared_ptr<WsBase> session,
    T& subscribers,
    boost::asio::io_context::strand& strand)
{
    boost::asio::post(strand, [&subscribers, s = std::move(session)]() {
        subscribers.erase(s);
    });
}

void
Subscription::subscribe(std::shared_ptr<WsBase> const& session)
{
    addSession(session, subscribers_, strand_);
}

void
Subscription::unsubscribe(std::shared_ptr<WsBase> const& session)
{
    removeSession(session, subscribers_, strand_);
}

void
Subscription::publish(std::string const& message)
{
    sendToSubscribers(message, subscribers_, strand_);
}

template <class Key>
void
SubscriptionMap<Key>::subscribe(
    std::shared_ptr<WsBase> const& session,
    Key const& account)
{
    addSession(session, subscribers_[account], strand_);
}

template <class Key>
void
SubscriptionMap<Key>::unsubscribe(
    std::shared_ptr<WsBase> const& session,
    Key const& account)
{
    removeSession(session, subscribers_[account], strand_);
}

template <class Key>
void
SubscriptionMap<Key>::publish(std::string const& message, Key const& account)
{
    sendToSubscribers(message, subscribers_[account], strand_);
}

boost::json::object
getLedgerPubMessage(
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
    pubMsg["reserve_base"] = RPC::toBoostJson(fees.reserve.jsonClipped());
    pubMsg["reserve_inc"] = RPC::toBoostJson(fees.increment.jsonClipped());

    pubMsg["validated_ledgers"] = ledgerRange;
    pubMsg["txn_count"] = txnCount;
    return pubMsg;
}

boost::json::object
SubscriptionManager::subLedger(std::shared_ptr<WsBase>& session)
{
    ledgerSubscribers_.subscribe(session);

    auto ledgerRange = backend_->fetchLedgerRange();
    assert(ledgerRange);
    auto lgrInfo = backend_->fetchLedgerBySequence(ledgerRange->maxSequence);
    assert(lgrInfo);

    std::optional<ripple::Fees> fees;
    fees = backend_->fetchFees(lgrInfo->seq);
    assert(fees);

    std::string range = std::to_string(ledgerRange->minSequence) + "-" +
        std::to_string(ledgerRange->maxSequence);

    auto pubMsg = getLedgerPubMessage(*lgrInfo, *fees, range, 0);
    pubMsg.erase("txn_count");
    pubMsg.erase("type");
    return pubMsg;
}

void
SubscriptionManager::unsubLedger(std::shared_ptr<WsBase>& session)
{
    ledgerSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::subTransactions(std::shared_ptr<WsBase>& session)
{
    txSubscribers_.subscribe(session);
}

void
SubscriptionManager::unsubTransactions(std::shared_ptr<WsBase>& session)
{
    txSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::subAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    accountSubscribers_.subscribe(session, account);
}

void
SubscriptionManager::unsubAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    accountSubscribers_.unsubscribe(session, account);
}

void
SubscriptionManager::subBook(
    ripple::Book const& book,
    std::shared_ptr<WsBase>& session)
{
    bookSubscribers_.subscribe(session, book);
}

void
SubscriptionManager::unsubBook(
    ripple::Book const& book,
    std::shared_ptr<WsBase>& session)
{
    bookSubscribers_.unsubscribe(session, book);
}

void
SubscriptionManager::pubLedger(
    ripple::LedgerInfo const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t txnCount)
{
    ledgerSubscribers_.publish(boost::json::serialize(
        getLedgerPubMessage(lgrInfo, fees, ledgerRange, txnCount)));
}

void
SubscriptionManager::pubTransaction(
    Backend::TransactionAndMetadata const& blobs,
    ripple::LedgerInfo const& lgrInfo)
{
    auto [tx, meta] = RPC::deserializeTxPlusMeta(blobs, lgrInfo.seq);
    boost::json::object pubObj;
    pubObj["transaction"] = RPC::toJson(*tx);
    pubObj["meta"] = RPC::toJson(*meta);
    RPC::insertDeliveredAmount(pubObj["meta"].as_object(), tx, meta);
    pubObj["type"] = "transaction";
    pubObj["validated"] = true;
    pubObj["status"] = "closed";

    pubObj["ledger_index"] = lgrInfo.seq;
    pubObj["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    pubObj["transaction"].as_object()["date"] =
        lgrInfo.closeTime.time_since_epoch().count();

    pubObj["engine_result_code"] = meta->getResult();
    std::string token;
    std::string human;
    ripple::transResultInfo(meta->getResultTER(), token, human);
    pubObj["engine_result"] = token;
    pubObj["engine_result_message"] = human;
    if (tx->getTxnType() == ripple::ttOFFER_CREATE)
    {
        auto account = tx->getAccountID(ripple::sfAccount);
        auto amount = tx->getFieldAmount(ripple::sfTakerGets);
        if (account != amount.issue().account)
        {
            auto ownerFunds =
                RPC::accountFunds(*backend_, lgrInfo.seq, amount, account);
            pubObj["transaction"].as_object()["owner_funds"] =
                ownerFunds.getText();
        }
    }

    std::string pubMsg{boost::json::serialize(pubObj)};
    txSubscribers_.publish(pubMsg);

    auto journal = ripple::debugLog();
    auto accounts = meta->getAffectedAccounts(journal);

    for (auto const& account : accounts)
        accountSubscribers_.publish(pubMsg, account);

    std::unordered_set<ripple::Book> alreadySent;

    for (auto const& node : meta->getNodes())
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
                    if (alreadySent.find(book) == alreadySent.end())
                    {
                        bookSubscribers_.publish(pubMsg, book);
                        alreadySent.insert(book);
                    }
                }
            }
        }
    }
}

void
SubscriptionManager::forwardProposedTransaction(
    boost::json::object const& response)
{
    std::string pubMsg{boost::json::serialize(response)};
    txProposedSubscribers_.publish(pubMsg);

    auto transaction = response.at("transaction").as_object();
    auto accounts = RPC::getAccountsFromTransaction(transaction);

    for (ripple::AccountID const& account : accounts)
        accountProposedSubscribers_.publish(pubMsg, account);
}

void
SubscriptionManager::forwardManifest(boost::json::object const& response)
{
    std::string pubMsg{boost::json::serialize(response)};
    manifestSubscribers_.publish(pubMsg);
}

void
SubscriptionManager::forwardValidation(boost::json::object const& response)
{
    std::string pubMsg{boost::json::serialize(response)};
    validationsSubscribers_.publish(std::move(pubMsg));
}

void
SubscriptionManager::subProposedAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    accountProposedSubscribers_.subscribe(session, account);
}

void
SubscriptionManager::subManifest(std::shared_ptr<WsBase>& session)
{
    manifestSubscribers_.subscribe(session);
}

void
SubscriptionManager::unsubManifest(std::shared_ptr<WsBase>& session)
{
    manifestSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::subValidation(std::shared_ptr<WsBase>& session)
{
    validationsSubscribers_.subscribe(session);
}

void
SubscriptionManager::unsubValidation(std::shared_ptr<WsBase>& session)
{
    validationsSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::unsubProposedAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    accountProposedSubscribers_.unsubscribe(session, account);
}

void
SubscriptionManager::subProposedTransactions(std::shared_ptr<WsBase>& session)
{
    txProposedSubscribers_.subscribe(session);
}

void
SubscriptionManager::unsubProposedTransactions(std::shared_ptr<WsBase>& session)
{
    txProposedSubscribers_.unsubscribe(session);
}
