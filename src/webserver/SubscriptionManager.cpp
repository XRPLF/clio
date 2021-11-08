#include <rpc/RPCHelpers.h>
#include <webserver/SubscriptionManager.h>
#include <webserver/WsBase.h>

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
    streams_[Ledgers].post([this, session](){
        streamSubscribers_[Ledgers].emplace(session);
    });
    
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
    streams_[Ledgers].post([this, session](){
        streamSubscribers_[Ledgers].erase(session);
    });
}

void
SubscriptionManager::subTransactions(std::shared_ptr<WsBase>& session)
{
    streams_[Transactions].post([this, session](){
        streamSubscribers_[Transactions].emplace(session);
    });
}

void
SubscriptionManager::unsubTransactions(std::shared_ptr<WsBase>& session)
{
    streams_[Transactions].post([this, session](){
        streamSubscribers_[Transactions].erase(session);
    });
}

void
SubscriptionManager::subAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    strand_.post([this, session](){
        accountSubscribers_[account].emplace(session);
    });
}

void
SubscriptionManager::unsubAccount(
    ripple::AccountID const& account,
    std::shared_ptr<WsBase>& session)
{
    strand_.post([this, session](){
        accountSubscribers_[account].erase(session);
    });
}

void
SubscriptionManager::subBook(
    ripple::Book const& book,
    std::shared_ptr<WsBase>& session)
{
    strand_.post([this, session](){
        bookSubscribers_[book].emplace(session);
    });
}

void
SubscriptionManager::unsubBook(
    ripple::Book const& book,
    std::shared_ptr<WsBase>& session)
{
    strand_.post([this, &session](){
        bookSubscribers_[book].erase(session);
    });
}

void
SubscriptionManager::sendAll(
    std::string const& pubMsg,
    std::unordered_set<std::shared_ptr<WsBase>>& subs
    )
{
    for (auto it = subs.begin(); it != subs.end();)
    {
        auto& session = *it;
        if (session->dead())
        {
            it = subs.erase(it);
        }
        else
        {
            session->send(pubMsg);
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
    streams_[Ledgers].post([this, lgrInfo, fees, ledgerRange, txnCount](){
        sendAll(
            boost::json::serialize(
                getLedgerPubMessage(lgrInfo, fees, ledgerRange, txnCount)),
            streamSubscribers_[Ledgers]);
    });
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
    streams_[Transactions].post([this, pubMsg](){
        sendAll(pubMsg, streamSubscribers_[Transactions]);
    });

    auto journal = ripple::debugLog();
    auto accounts = meta->getAffectedAccounts(journal);

    for (ripple::AccountID const& account : accounts)
        strand_.post([this, account, pubMsg](){
            sendAll(pubMsg, accountSubscribers_[account]);
        });

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
                        strand_.post([pubMsg, book, this](){
                            sendAll(pubMsg, bookSubscribers_[book]);
                        });
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
    streams_[TransactionsProposed].post([account, pubMsg, this](){
        sendAll(pubMsg, streamSubscribers_[TransactionsProposed]);
    });

    auto transaction = response.at("transaction").as_object();
    auto accounts = RPC::getAccountsFromTransaction(transaction);

    for (ripple::AccountID const& account : accounts)
        strand_.post([account, pubMsg, this](){
            sendAll(pubMsg, accountProposedSubscribers_[account]);
        });
}

void 
SubscriptionManager::forwardManifest(boost::json::object const& response)
{
    std::string pubMsg{boost::json::serialize(response)};
    streams_[Manifests].post([m = std::move(pubMsg), this](){
        sendAll(m, streamSubscribers_[Manifests]);
    });
}

void 
SubscriptionManager::forwardValidation(boost::json::object const& response)
{
    std::string pubMsg{boost::json::serialize(response)};
    streams_[Validations].post([m = std::move(pubMsg), this](){
        sendAll(m, streamSubscribers_[Validations]);
    });
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
SubscriptionManager::subManifest(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[Manifests].emplace(session);
}

void
SubscriptionManager::unsubManifest(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[Manifests].erase(session);
}

void
SubscriptionManager::subValidation(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[Validations].emplace(session);
}

void
SubscriptionManager::unsubValidation(std::shared_ptr<WsBase>& session)
{
    std::lock_guard lk(m_);
    streamSubscribers_[Validations].emplace(session);
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

