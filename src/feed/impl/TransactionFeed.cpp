//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "feed/impl/TransactionFeed.hpp"

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "feed/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace feed::impl {

void
TransactionFeed::TransactionSlot::operator()(AllVersionTransactionsType const& allVersionMsgs) const
{
    if (auto connection = connectionWeakPtr.lock(); connection) {
        // Check if this connection already sent
        if (feed.get().notified_.contains(connection.get()))
            return;

        feed.get().notified_.insert(connection.get());

        if (connection->apiSubVersion < 2u) {
            connection->send(allVersionMsgs[0]);
            return;
        }
        connection->send(allVersionMsgs[1]);
    }
}

void
TransactionFeed::sub(SubscriberSharedPtr const& subscriber)
{
    auto const added = signal_.connectTrackableSlot(subscriber, TransactionSlot(*this, subscriber));
    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed transactions";
        ++subAllCount_.get();
        subscriber->onDisconnect.connect([this](SubscriberPtr connection) { unsubInternal(connection); });
    }
}

void
TransactionFeed::sub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    auto const added = accountSignal_.connectTrackableSlot(subscriber, account, TransactionSlot(*this, subscriber));
    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed account " << account;
        ++subAccountCount_.get();
        subscriber->onDisconnect.connect([this, account](SubscriberPtr connection) {
            unsubInternal(account, connection);
        });
    }
}

void
TransactionFeed::subProposed(SubscriberSharedPtr const& subscriber)
{
    auto const added = txProposedsignal_.connectTrackableSlot(subscriber, TransactionSlot(*this, subscriber));
    if (added) {
        subscriber->onDisconnect.connect([this](SubscriberPtr connection) { unsubProposedInternal(connection); });
    }
}

void
TransactionFeed::subProposed(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    auto const added =
        accountProposedSignal_.connectTrackableSlot(subscriber, account, TransactionSlot(*this, subscriber));
    if (added) {
        subscriber->onDisconnect.connect([this, account](SubscriberPtr connection) {
            unsubProposedInternal(account, connection);
        });
    }
}

void
TransactionFeed::sub(ripple::Book const& book, SubscriberSharedPtr const& subscriber)
{
    auto const added = bookSignal_.connectTrackableSlot(subscriber, book, TransactionSlot(*this, subscriber));
    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed book " << book;
        ++subBookCount_.get();
        subscriber->onDisconnect.connect([this, book](SubscriberPtr connection) { unsubInternal(book, connection); });
    }
}

void
TransactionFeed::unsub(SubscriberSharedPtr const& subscriber)
{
    unsubInternal(subscriber.get());
}

void
TransactionFeed::unsub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    unsubInternal(account, subscriber.get());
}

void
TransactionFeed::unsubProposed(SubscriberSharedPtr const& subscriber)
{
    unsubProposedInternal(subscriber.get());
}

void
TransactionFeed::unsubProposed(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    unsubProposedInternal(account, subscriber.get());
}

void
TransactionFeed::unsub(ripple::Book const& book, SubscriberSharedPtr const& subscriber)
{
    unsubInternal(book, subscriber.get());
}

std::uint64_t
TransactionFeed::transactionSubCount() const
{
    return subAllCount_.get().value();
}

std::uint64_t
TransactionFeed::accountSubCount() const
{
    return subAccountCount_.get().value();
}

std::uint64_t
TransactionFeed::bookSubCount() const
{
    return subBookCount_.get().value();
}

void
TransactionFeed::pub(
    data::TransactionAndMetadata const& txMeta,
    ripple::LedgerHeader const& lgrInfo,
    std::shared_ptr<data::BackendInterface const> const& backend
)
{
    auto [tx, meta] = rpc::deserializeTxPlusMeta(txMeta, lgrInfo.seq);

    std::optional<ripple::STAmount> ownerFunds;

    if (tx->getTxnType() == ripple::ttOFFER_CREATE) {
        auto const account = tx->getAccountID(ripple::sfAccount);
        auto const amount = tx->getFieldAmount(ripple::sfTakerGets);
        if (account != amount.issue().account) {
            auto fetchFundsSynchronous = [&]() {
                data::synchronous([&](boost::asio::yield_context yield) {
                    ownerFunds = rpc::accountFunds(*backend, lgrInfo.seq, amount, account, yield);
                });
            };
            data::retryOnTimeout(fetchFundsSynchronous);
        }
    }

    auto const genJsonByVersion = [&, tx, meta](std::uint32_t version) {
        boost::json::object pubObj;
        auto const txKey = version < 2u ? JS(transaction) : JS(tx_json);
        pubObj[txKey] = rpc::toJson(*tx);
        pubObj[JS(meta)] = rpc::toJson(*meta);
        rpc::insertDeliveredAmount(pubObj[JS(meta)].as_object(), tx, meta, txMeta.date);
        rpc::insertDeliverMaxAlias(pubObj[txKey].as_object(), version);
        rpc::insertMPTIssuanceID(pubObj[JS(meta)].as_object(), tx, meta);

        pubObj[JS(type)] = "transaction";
        pubObj[JS(validated)] = true;
        pubObj[JS(status)] = "closed";
        pubObj[JS(close_time_iso)] = ripple::to_string_iso(lgrInfo.closeTime);

        pubObj[JS(ledger_index)] = lgrInfo.seq;
        pubObj[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
        if (version >= 2u) {
            if (pubObj[txKey].as_object().contains(JS(hash))) {
                pubObj[JS(hash)] = pubObj[txKey].as_object()[JS(hash)];
                pubObj[txKey].as_object().erase(JS(hash));
            }
        }
        pubObj[txKey].as_object()[JS(date)] = lgrInfo.closeTime.time_since_epoch().count();

        pubObj[JS(engine_result_code)] = meta->getResult();
        std::string token;
        std::string human;
        ripple::transResultInfo(meta->getResultTER(), token, human);
        pubObj[JS(engine_result)] = token;
        pubObj[JS(engine_result_message)] = human;

        if (ownerFunds)
            pubObj[txKey].as_object()[JS(owner_funds)] = ownerFunds->getText();

        return pubObj;
    };

    AllVersionTransactionsType allVersionsMsgs{
        std::make_shared<std::string>(boost::json::serialize(genJsonByVersion(1u))),
        std::make_shared<std::string>(boost::json::serialize(genJsonByVersion(2u)))
    };

    auto const affectedAccountsFlat = meta->getAffectedAccounts();
    auto affectedAccounts =
        std::unordered_set<ripple::AccountID>(affectedAccountsFlat.cbegin(), affectedAccountsFlat.cend());

    std::unordered_set<ripple::Book> affectedBooks;

    for (auto const& node : meta->getNodes()) {
        if (node.getFieldU16(ripple::sfLedgerEntryType) == ripple::ltOFFER) {
            ripple::SField const* field = nullptr;

            // We need a field that contains the TakerGets and TakerPays
            // parameters.
            if (node.getFName() == ripple::sfModifiedNode) {
                field = &ripple::sfPreviousFields;
            } else if (node.getFName() == ripple::sfCreatedNode) {
                field = &ripple::sfNewFields;
            } else if (node.getFName() == ripple::sfDeletedNode) {
                field = &ripple::sfFinalFields;
            }

            if (field != nullptr) {
                auto const data = dynamic_cast<ripple::STObject const*>(node.peekAtPField(*field));

                if ((data != nullptr) && data->isFieldPresent(ripple::sfTakerPays) &&
                    data->isFieldPresent(ripple::sfTakerGets)) {
                    // determine the OrderBook
                    ripple::Book const book{
                        data->getFieldAmount(ripple::sfTakerGets).issue(),
                        data->getFieldAmount(ripple::sfTakerPays).issue()
                    };
                    if (affectedBooks.find(book) == affectedBooks.end()) {
                        affectedBooks.insert(book);
                    }
                }
            }
        }
    }

    [[maybe_unused]] auto task = strand_.execute([this,
                                                  allVersionsMsgs = std::move(allVersionsMsgs),
                                                  affectedAccounts = std::move(affectedAccounts),
                                                  affectedBooks = std::move(affectedBooks)]() {
        notified_.clear();
        signal_.emit(allVersionsMsgs);
        // clear the notified set. If the same connection subscribes both transactions + proposed_transactions,
        // rippled SENDS the same message twice
        notified_.clear();
        txProposedsignal_.emit(allVersionsMsgs);
        notified_.clear();
        // check duplicate for account and proposed_account, this prevents sending the same message multiple times
        // if it affects multiple accounts watched by the same connection
        for (auto const& account : affectedAccounts) {
            accountSignal_.emit(account, allVersionsMsgs);
            accountProposedSignal_.emit(account, allVersionsMsgs);
        }
        notified_.clear();
        // check duplicate for books, this prevents sending the same message multiple times if it affects multiple
        // books watched by the same connection
        for (auto const& book : affectedBooks) {
            bookSignal_.emit(book, allVersionsMsgs);
        }
    });
}

void
TransactionFeed::unsubInternal(SubscriberPtr subscriber)
{
    if (signal_.disconnect(subscriber)) {
        LOG(logger_.info()) << subscriber->tag() << "Unsubscribed transactions";
        --subAllCount_.get();
    }
}

void
TransactionFeed::unsubInternal(ripple::AccountID const& account, SubscriberPtr subscriber)
{
    if (accountSignal_.disconnect(subscriber, account)) {
        LOG(logger_.info()) << subscriber->tag() << "Unsubscribed account " << account;
        --subAccountCount_.get();
    }
}

void
TransactionFeed::unsubProposedInternal(SubscriberPtr subscriber)
{
    txProposedsignal_.disconnect(subscriber);
}

void
TransactionFeed::unsubProposedInternal(ripple::AccountID const& account, SubscriberPtr subscriber)
{
    accountProposedSignal_.disconnect(subscriber, account);
}

void
TransactionFeed::unsubInternal(ripple::Book const& book, SubscriberPtr subscriber)
{
    if (bookSignal_.disconnect(subscriber, book)) {
        LOG(logger_.info()) << subscriber->tag() << "Unsubscribed book " << book;
        --subBookCount_.get();
    }
}
}  // namespace feed::impl
