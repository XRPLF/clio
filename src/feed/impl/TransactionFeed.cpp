#include "feed/impl/TransactionFeed.hpp"

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "feed/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/Assert.hpp"
#include "util/JsonUtils.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/json/json_value.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/NFTSyntheticSerializer.h>
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
TransactionFeed::TransactionSlot::operator()(
    std::shared_ptr<AllVersionsMsgsType> const& allVersionMsgs
) const
{
    if (auto connection = subscriptionContextWeakPtr.lock(); connection) {
        // Check if this connection already sent
        if (feed.get().notified_.contains(connection.get()))
            return;

        feed.get().notified_.insert(connection.get());

        if (connection->apiSubversion() < 2u) {
            connection->send(std::shared_ptr<std::string>(allVersionMsgs, &allVersionMsgs->v1));
            return;
        }
        connection->send(std::shared_ptr<std::string>(allVersionMsgs, &allVersionMsgs->v2));
    }
}

void
TransactionFeed::sub(SubscriberSharedPtr const& subscriber)
{
    auto const added = signal_.connectTrackableSlot(subscriber, TransactionSlot(*this, subscriber));
    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed transactions";
        ++subAllCount_.get();
        subscriber->onDisconnect([this](SubscriberPtr connection) { unsubInternal(connection); });
    }
}

void
TransactionFeed::sub(xrpl::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    auto const added = accountSignal_.connectTrackableSlot(
        subscriber, account, TransactionSlot(*this, subscriber)
    );
    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed account " << account;
        ++subAccountCount_.get();
        subscriber->onDisconnect([this, account](SubscriberPtr connection) {
            unsubInternal(account, connection);
        });
    }
}

void
TransactionFeed::subProposed(SubscriberSharedPtr const& subscriber)
{
    auto const added =
        txProposedSignal_.connectTrackableSlot(subscriber, TransactionSlot(*this, subscriber));
    if (added) {
        subscriber->onDisconnect([this](SubscriberPtr connection) {
            unsubProposedInternal(connection);
        });
    }
}

void
TransactionFeed::subProposed(xrpl::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    auto const added = accountProposedSignal_.connectTrackableSlot(
        subscriber, account, TransactionSlot(*this, subscriber)
    );
    if (added) {
        subscriber->onDisconnect([this, account](SubscriberPtr connection) {
            unsubProposedInternal(account, connection);
        });
    }
}

void
TransactionFeed::sub(xrpl::Book const& book, SubscriberSharedPtr const& subscriber)
{
    auto const added =
        bookSignal_.connectTrackableSlot(subscriber, book, TransactionSlot(*this, subscriber));
    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed book " << book;
        ++subBookCount_.get();
        subscriber->onDisconnect([this, book](SubscriberPtr connection) {
            unsubInternal(book, connection);
        });
    }
}

void
TransactionFeed::unsub(SubscriberSharedPtr const& subscriber)
{
    unsubInternal(subscriber.get());
}

void
TransactionFeed::unsub(xrpl::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    unsubInternal(account, subscriber.get());
}

void
TransactionFeed::unsubProposed(SubscriberSharedPtr const& subscriber)
{
    unsubProposedInternal(subscriber.get());
}

void
TransactionFeed::unsubProposed(
    xrpl::AccountID const& account,
    SubscriberSharedPtr const& subscriber
)
{
    unsubProposedInternal(account, subscriber.get());
}

void
TransactionFeed::unsub(xrpl::Book const& book, SubscriberSharedPtr const& subscriber)
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
    xrpl::LedgerHeader const& lgrInfo,
    std::shared_ptr<data::BackendInterface const> const& backend,
    std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter,
    uint32_t const networkID
)
{
    auto [tx, meta] = rpc::deserializeTxPlusMeta(txMeta, lgrInfo.seq);

    std::optional<xrpl::STAmount> ownerFunds;

    if (tx->getTxnType() == xrpl::ttOFFER_CREATE) {
        auto const account = tx->getAccountID(xrpl::sfAccount);
        auto const amount = tx->getFieldAmount(xrpl::sfTakerGets);
        if (account != amount.get<xrpl::Issue>().account) {
            auto fetchFundsSynchronous = [&]() {
                data::synchronous([&](boost::asio::yield_context yield) {
                    ownerFunds = rpc::accountFunds(
                        *backend, *amendmentCenter, lgrInfo.seq, amount, account, yield
                    );
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

        auto& txnPubobj = pubObj[txKey].as_object();
        auto& metaPubobj = pubObj[JS(meta)].as_object();
        rpc::insertDeliverMaxAlias(txnPubobj, version);
        rpc::insertMPTIssuanceID(txnPubobj, tx, metaPubobj, meta);

        json::Value nftJson;
        xrpl::RPC::insertNFTSyntheticInJson(nftJson, tx, *meta);
        auto const nftBoostJson = rpc::toBoostJson(nftJson).as_object();
        if (nftBoostJson.contains(JS(meta)) && nftBoostJson.at(JS(meta)).is_object()) {
            auto& metaObjInPub = pubObj.at(JS(meta)).as_object();
            for (auto const& [k, v] : nftBoostJson.at(JS(meta)).as_object())
                metaObjInPub.insert_or_assign(k, v);
        }

        auto const& metaObj = pubObj[JS(meta)];
        ASSERT(metaObj.is_object(), "meta must be an obj in rippled and clio");
        if (metaObj.as_object().contains("TransactionIndex") &&
            metaObj.as_object().at("TransactionIndex").is_int64()) {
            if (auto const& ctid = rpc::encodeCTID(
                    lgrInfo.seq,
                    util::integralValueAs<uint16_t>(metaObj.as_object().at("TransactionIndex")),
                    networkID
                );
                ctid)
                pubObj[JS(ctid)] = *ctid;
        }

        pubObj[JS(type)] = "transaction";
        pubObj[JS(validated)] = true;
        pubObj[JS(status)] = "closed";
        pubObj[JS(close_time_iso)] = xrpl::toStringIso(lgrInfo.closeTime);

        pubObj[JS(ledger_index)] = lgrInfo.seq;
        pubObj[JS(ledger_hash)] = xrpl::strHex(lgrInfo.hash);
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
        xrpl::transResultInfo(meta->getResultTER(), token, human);
        pubObj[JS(engine_result)] = token;
        pubObj[JS(engine_result_message)] = human;

        if (ownerFunds)
            pubObj[txKey].as_object()[JS(owner_funds)] = ownerFunds->getText();

        return pubObj;
    };

    auto allVersionsMsgs = std::make_shared<AllVersionsMsgsType>(
        boost::json::serialize(genJsonByVersion(1u)), boost::json::serialize(genJsonByVersion(2u))
    );

    auto const affectedAccountsFlat = meta->getAffectedAccounts();
    auto affectedAccounts = std::unordered_set<xrpl::AccountID>(
        affectedAccountsFlat.cbegin(), affectedAccountsFlat.cend()
    );

    std::unordered_set<xrpl::Book> affectedBooks;

    for (auto const& node : meta->getNodes()) {
        if (node.getFieldU16(xrpl::sfLedgerEntryType) == xrpl::ltOFFER) {
            xrpl::SField const* field = nullptr;

            // We need a field that contains the TakerGets and TakerPays
            // parameters.
            if (node.getFName() == xrpl::sfModifiedNode) {
                field = &xrpl::sfPreviousFields;
            } else if (node.getFName() == xrpl::sfCreatedNode) {
                field = &xrpl::sfNewFields;
            } else if (node.getFName() == xrpl::sfDeletedNode) {
                field = &xrpl::sfFinalFields;
            }

            if (field != nullptr) {
                auto const data = dynamic_cast<xrpl::STObject const*>(node.peekAtPField(*field));

                if ((data != nullptr) && data->isFieldPresent(xrpl::sfTakerPays) &&
                    data->isFieldPresent(xrpl::sfTakerGets)) {
                    // determine the OrderBook
                    xrpl::Book const book{
                        data->getFieldAmount(xrpl::sfTakerGets).get<xrpl::Issue>(),
                        data->getFieldAmount(xrpl::sfTakerPays).get<xrpl::Issue>(),
                        (*data)[~xrpl::sfDomainID]
                    };
                    if (!affectedBooks.contains(book)) {
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
        // clear the notified set. If the same connection subscribes both transactions +
        // proposed_transactions, rippled SENDS the same message twice
        notified_.clear();
        txProposedSignal_.emit(allVersionsMsgs);
        notified_.clear();
        // check duplicate for account and proposed_account, this prevents sending the same message
        // multiple times if it affects multiple accounts watched by the same connection
        for (auto const& account : affectedAccounts) {
            accountSignal_.emit(account, allVersionsMsgs);
            accountProposedSignal_.emit(account, allVersionsMsgs);
        }
        notified_.clear();
        // check duplicate for books, this prevents sending the same message multiple times if it
        // affects multiple books watched by the same connection
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
TransactionFeed::unsubInternal(xrpl::AccountID const& account, SubscriberPtr subscriber)
{
    if (accountSignal_.disconnect(subscriber, account)) {
        LOG(logger_.info()) << subscriber->tag() << "Unsubscribed account " << account;
        --subAccountCount_.get();
    }
}

void
TransactionFeed::unsubProposedInternal(SubscriberPtr subscriber)
{
    txProposedSignal_.disconnect(subscriber);
}

void
TransactionFeed::unsubProposedInternal(xrpl::AccountID const& account, SubscriberPtr subscriber)
{
    accountProposedSignal_.disconnect(subscriber, account);
}

void
TransactionFeed::unsubInternal(xrpl::Book const& book, SubscriberPtr subscriber)
{
    if (bookSignal_.disconnect(subscriber, book)) {
        LOG(logger_.info()) << subscriber->tag() << "Unsubscribed book " << book;
        --subBookCount_.get();
    }
}
}  // namespace feed::impl
