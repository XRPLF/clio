#include <rpc/RPCHelpers.h>
#include <subscriptions/SubscriptionManager.h>
#include <webserver/WsBase.h>

void
Subscription::sendAll(std::string const& pubMsg)
{
    for (auto it = subscribers_.begin(); it != subscribers_.end();)
    {
        std::cout << "PUBLISHING TO SUBSCRIBER" << std::endl;
        auto& session = *it;
        if (session->dead())
        {
            std::cout << "SESSION IS DEAD" << std::endl;
            it = subscribers_.erase(it);
        }
        else
        {
            session->send(msg);
            ++it;
            std::cout << "DONE SENDING" << std::endl;
        }
    }
}

void
Subscription::subscribe(std::shared_ptr<WsBase> const& session)
{
    std::cout << "SUBSCRIBING" << std::endl;
    boost::asio::post(strand_, [this, session](){
        std::cout << "EMPLACING " << std::endl;
        subscribers_.emplace(session);
        std::cout << "EMPLACED" << std::endl;
    });
}

void
Subscription::unsubscribe(std::shared_ptr<WsBase> const& session)
{
    boost::asio::post(strand_, [this, session](){
        std::cout << "ERASING" << std::endl;
        subscribers_.erase(session);
        std::cout << "ERASED" << std::endl;
    });
}

void
Subscription::publish(std::string const& message)
{
    boost::asio::post(strand_, [this, message](){
        sendAll(message);
    });
}

void
AccountSubscription::subscribe(
    std::shared_ptr<WsBase> const& session,
    ripple::AccountID const& account)
{
    if (strand_.running_in_this_thread())
        return;

    boost::asio::post(strand_, [this, session, account](){
        subscribers_[account].emplace(session);
    });
}

void
AccountSubscription::unsubscribe(
    std::shared_ptr<WsBase> const& session,
    ripple::AccountID const& account)
{
    if (strand_.running_in_this_thread())
        return;

    boost::asio::post(strand_, [this, session, account](){
        subscribers_[account].erase(session);
    });
}

void
AccountSubscription::publish(
    std::string const& message,
    ripple::AccountID const& account)
{
    if (strand_.running_in_this_thread())
        return;

    boost::asio::post(strand_, [this, message, account]() {
        sendAll(message, account);
    });
}

void
AccountSubscription::sendAll(
    std::string const& pubMsg,
    ripple::AccountID const& account)
{
    for (auto it = subscribers_[account].begin(); it != subscribers_[account].end();)
    {
        auto& session = *it;
        if (session->dead())
        {
            it = subscribers_[account].erase(it);
        }
        else
        {
            session->send(pubMsg);
            ++it;
        }
    }
}

void
BookSubscription::sendAll(std::string const& pubMsg, ripple::Book const& book)
{
    for (auto it = subscribers_[book].begin(); it != subscribers_[book].end();)
    {
        auto& session = *it;
        if (session->dead())
        {
            it = subscribers_[book].erase(it);
        }
        else
        {
            session->send(pubMsg);
            ++it;
        }
    }
}

void
BookSubscription::subscribe(
    std::shared_ptr<WsBase> const& session,
    ripple::Book const& book)
{
    if (strand_.running_in_this_thread())
        return;

    boost::asio::post(strand_, [this, session, book](){
        subscribers_[book].emplace(session);
    });
}

void
BookSubscription::unsubscribe(
    std::shared_ptr<WsBase> const& session,
    ripple::Book const& book)
{
    if (strand_.running_in_this_thread())
        return;

    boost::asio::post(strand_, [this, session, book](){
        subscribers_[book].erase(session);
    });
}

void
BookSubscription::publish(
    std::string const& message,
    ripple::Book const& book)
{
    if (strand_.running_in_this_thread())
        return;

    boost::asio::post(strand_, [this, message, book](){
        sendAll(message, book);
    });
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
    streamSubscribers_[Ledgers]->subscribe(session);
    
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
    streamSubscribers_[Ledgers]->unsubscribe(session);
}

void
SubscriptionManager::subTransactions(std::shared_ptr<WsBase>& session)
{
    streamSubscribers_[Transactions]->subscribe(session);
}

void
SubscriptionManager::unsubTransactions(std::shared_ptr<WsBase>& session)
{
    streamSubscribers_[Transactions]->unsubscribe(session);
}

void
SubscriptionManager::subAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    accountSubscribers_->subscribe(session, account);
}

void
SubscriptionManager::unsubAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    accountSubscribers_->unsubscribe(session, account);
}

void
SubscriptionManager::subBook(
    ripple::Book const& book,
    std::shared_ptr<WsBase>& session)
{
    bookSubscribers_->subscribe(session, book);
}

void
SubscriptionManager::unsubBook(
    ripple::Book const& book,
    std::shared_ptr<WsBase>& session)
{
    bookSubscribers_->unsubscribe(session, book);
}

void
SubscriptionManager::pubLedger(
    ripple::LedgerInfo const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t txnCount)
{
    streamSubscribers_[Ledgers]->publish(boost::json::serialize(
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
    streamSubscribers_[Transactions]->publish(pubMsg);

    auto journal = ripple::debugLog();
    auto accounts = meta->getAffectedAccounts(journal);

    for (auto const& account : accounts)
        accountSubscribers_->publish(pubMsg, account);

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
                    if (!alreadySent.contains(book))
                    {
                        bookSubscribers_->publish(pubMsg, book);
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
    streamSubscribers_[TransactionsProposed]->publish(pubMsg);

    auto transaction = response.at("transaction").as_object();
    auto accounts = RPC::getAccountsFromTransaction(transaction);

    for (ripple::AccountID const& account : accounts)
       accountProposedSubscribers_->publish(pubMsg, account);
}

void 
SubscriptionManager::forwardManifest(boost::json::object const& response)
{
    std::string pubMsg{boost::json::serialize(response)};
    streamSubscribers_[Manifests]->publish(pubMsg);
}

void 
SubscriptionManager::forwardValidation(boost::json::object const& response)
{
    std::string pubMsg{boost::json::serialize(response)};
    streamSubscribers_[Validations]->publish(std::move(pubMsg));
}

void
SubscriptionManager::subProposedAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    accountProposedSubscribers_->subscribe(session, account);
}

void
SubscriptionManager::subManifest(std::shared_ptr<WsBase>& session)
{
    streamSubscribers_[Manifests]->subscribe(session);
}

void
SubscriptionManager::unsubManifest(std::shared_ptr<WsBase>& session)
{
    streamSubscribers_[Manifests]->unsubscribe(session);
}

void
SubscriptionManager::subValidation(std::shared_ptr<WsBase>& session)
{
    std::cout << "SUBGBING VALS " << std::endl;
    streamSubscribers_[Validations]->subscribe(session);
}

void
SubscriptionManager::unsubValidation(std::shared_ptr<WsBase>& session)
{
    streamSubscribers_[Validations]->unsubscribe(session);
}

void
SubscriptionManager::unsubProposedAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    accountProposedSubscribers_->unsubscribe(session, account);
}

void
SubscriptionManager::subProposedTransactions(std::shared_ptr<WsBase>& session)
{
    streamSubscribers_[TransactionsProposed]->subscribe(session);
}

void
SubscriptionManager::unsubProposedTransactions(std::shared_ptr<WsBase>& session)
{
    streamSubscribers_[TransactionsProposed]->unsubscribe(session);
}

