//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <rpc/BookChangesHelper.h>
#include <rpc/RPCHelpers.h>
#include <subscriptions/SubscriptionManager.h>
#include <webserver2/interface/ConnectionBase.h>

void
Subscription::subscribe(SessionPtrType const& session)
{
    boost::asio::post(strand_, [this, session]() { addSession(session, subscribers_, subCount_); });
}

void
Subscription::unsubscribe(SessionPtrType const& session)
{
    boost::asio::post(strand_, [this, session]() { removeSession(session, subscribers_, subCount_); });
}

void
Subscription::publish(std::shared_ptr<std::string> const& message)
{
    boost::asio::post(strand_, [this, message]() { sendToSubscribers(message, subscribers_, subCount_); });
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
SubscriptionManager::subLedger(boost::asio::yield_context& yield, SessionPtrType session)
{
    subscribeHelper(session, ledgerSubscribers_, [this](SessionPtrType session) { unsubLedger(session); });

    auto ledgerRange = backend_->fetchLedgerRange();
    assert(ledgerRange);
    auto lgrInfo = backend_->fetchLedgerBySequence(ledgerRange->maxSequence, yield);
    assert(lgrInfo);

    std::optional<ripple::Fees> fees;
    fees = backend_->fetchFees(lgrInfo->seq, yield);
    assert(fees);

    std::string range = std::to_string(ledgerRange->minSequence) + "-" + std::to_string(ledgerRange->maxSequence);

    auto pubMsg = getLedgerPubMessage(*lgrInfo, *fees, range, 0);
    pubMsg.erase("txn_count");
    pubMsg.erase("type");
    return pubMsg;
}

void
SubscriptionManager::unsubLedger(SessionPtrType session)
{
    ledgerSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::subTransactions(SessionPtrType session)
{
    subscribeHelper(session, txSubscribers_, [this](SessionPtrType session) { unsubTransactions(session); });
}

void
SubscriptionManager::unsubTransactions(SessionPtrType session)
{
    txSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::subAccount(ripple::AccountID const& account, SessionPtrType const& session)
{
    subscribeHelper(session, account, accountSubscribers_, [this, account](SessionPtrType session) {
        unsubAccount(account, session);
    });
}

void
SubscriptionManager::unsubAccount(ripple::AccountID const& account, SessionPtrType const& session)
{
    accountSubscribers_.unsubscribe(session, account);
}

void
SubscriptionManager::subBook(ripple::Book const& book, SessionPtrType session)
{
    subscribeHelper(
        session, book, bookSubscribers_, [this, book](SessionPtrType session) { unsubBook(book, session); });
}

void
SubscriptionManager::unsubBook(ripple::Book const& book, SessionPtrType session)
{
    bookSubscribers_.unsubscribe(session, book);
}

void
SubscriptionManager::subBookChanges(SessionPtrType session)
{
    subscribeHelper(session, bookChangesSubscribers_, [this](SessionPtrType session) { unsubBookChanges(session); });
}

void
SubscriptionManager::unsubBookChanges(SessionPtrType session)
{
    bookChangesSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::pubLedger(
    ripple::LedgerInfo const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t txnCount)
{
    auto message = std::make_shared<std::string>(
        boost::json::serialize(getLedgerPubMessage(lgrInfo, fees, ledgerRange, txnCount)));

    ledgerSubscribers_.publish(message);
}

void
SubscriptionManager::pubTransaction(Backend::TransactionAndMetadata const& blobs, ripple::LedgerInfo const& lgrInfo)
{
    auto [tx, meta] = RPC::deserializeTxPlusMeta(blobs, lgrInfo.seq);
    boost::json::object pubObj;
    pubObj["transaction"] = RPC::toJson(*tx);
    pubObj["meta"] = RPC::toJson(*meta);
    RPC::insertDeliveredAmount(pubObj["meta"].as_object(), tx, meta, blobs.date);
    pubObj["type"] = "transaction";
    pubObj["validated"] = true;
    pubObj["status"] = "closed";

    pubObj["ledger_index"] = lgrInfo.seq;
    pubObj["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    pubObj["transaction"].as_object()["date"] = lgrInfo.closeTime.time_since_epoch().count();

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
            ripple::STAmount ownerFunds;
            auto fetchFundsSynchronous = [&]() {
                Backend::synchronous([&](boost::asio::yield_context& yield) {
                    ownerFunds = RPC::accountFunds(*backend_, lgrInfo.seq, amount, account, yield);
                });
            };

            Backend::retryOnTimeout(fetchFundsSynchronous);

            pubObj["transaction"].as_object()["owner_funds"] = ownerFunds.getText();
        }
    }

    auto pubMsg = std::make_shared<std::string>(boost::json::serialize(pubObj));
    txSubscribers_.publish(pubMsg);

    auto accounts = meta->getAffectedAccounts();

    for (auto const& account : accounts)
        accountSubscribers_.publish(pubMsg, account);

    std::unordered_set<ripple::Book> alreadySent;

    for (auto const& node : meta->getNodes())
    {
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
                auto data = dynamic_cast<const ripple::STObject*>(node.peekAtPField(*field));

                if (data && data->isFieldPresent(ripple::sfTakerPays) && data->isFieldPresent(ripple::sfTakerGets))
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
SubscriptionManager::pubBookChanges(
    ripple::LedgerInfo const& lgrInfo,
    std::vector<Backend::TransactionAndMetadata> const& transactions)
{
    auto const json = RPC::computeBookChanges(lgrInfo, transactions);
    auto const bookChangesMsg = std::make_shared<std::string>(boost::json::serialize(json));
    bookChangesSubscribers_.publish(bookChangesMsg);
}

void
SubscriptionManager::forwardProposedTransaction(boost::json::object const& response)
{
    auto pubMsg = std::make_shared<std::string>(boost::json::serialize(response));
    txProposedSubscribers_.publish(pubMsg);

    auto transaction = response.at("transaction").as_object();
    auto accounts = RPC::getAccountsFromTransaction(transaction);

    for (ripple::AccountID const& account : accounts)
        accountProposedSubscribers_.publish(pubMsg, account);
}

void
SubscriptionManager::forwardManifest(boost::json::object const& response)
{
    auto pubMsg = std::make_shared<std::string>(boost::json::serialize(response));
    manifestSubscribers_.publish(pubMsg);
}

void
SubscriptionManager::forwardValidation(boost::json::object const& response)
{
    auto pubMsg = std::make_shared<std::string>(boost::json::serialize(response));
    validationsSubscribers_.publish(pubMsg);
}

void
SubscriptionManager::subProposedAccount(ripple::AccountID const& account, SessionPtrType session)
{
    subscribeHelper(session, account, accountProposedSubscribers_, [this, account](SessionPtrType session) {
        unsubProposedAccount(account, session);
    });
}

void
SubscriptionManager::subManifest(SessionPtrType session)
{
    subscribeHelper(session, manifestSubscribers_, [this](SessionPtrType session) { unsubManifest(session); });
}

void
SubscriptionManager::unsubManifest(SessionPtrType session)
{
    manifestSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::subValidation(SessionPtrType session)
{
    subscribeHelper(session, validationsSubscribers_, [this](SessionPtrType session) { unsubValidation(session); });
}

void
SubscriptionManager::unsubValidation(SessionPtrType session)
{
    validationsSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::unsubProposedAccount(ripple::AccountID const& account, SessionPtrType session)
{
    accountProposedSubscribers_.unsubscribe(session, account);
}

void
SubscriptionManager::subProposedTransactions(SessionPtrType session)
{
    subscribeHelper(
        session, txProposedSubscribers_, [this](SessionPtrType session) { unsubProposedTransactions(session); });
}

void
SubscriptionManager::unsubProposedTransactions(SessionPtrType session)
{
    txProposedSubscribers_.unsubscribe(session);
}

void
SubscriptionManager::subscribeHelper(SessionPtrType const& session, Subscription& subs, CleanupFunction&& func)
{
    subs.subscribe(session);
    std::scoped_lock lk(cleanupMtx_);
    cleanupFuncs_[session].push_back(std::move(func));
}

template <typename Key>
void
SubscriptionManager::subscribeHelper(
    SessionPtrType const& session,
    Key const& k,
    SubscriptionMap<Key>& subs,
    CleanupFunction&& func)
{
    subs.subscribe(session, k);
    std::scoped_lock lk(cleanupMtx_);
    cleanupFuncs_[session].push_back(std::move(func));
}

void
SubscriptionManager::cleanup(SessionPtrType session)
{
    std::scoped_lock lk(cleanupMtx_);
    if (!cleanupFuncs_.contains(session))
        return;

    for (auto f : cleanupFuncs_[session])
    {
        f(session);
    }

    cleanupFuncs_.erase(session);
}
