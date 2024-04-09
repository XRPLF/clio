//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "rpc/common/impl/HandlerProvider.hpp"

#include "data/BackendInterface.hpp"
#include "etl/ETLService.hpp"
#include "feed/SubscriptionManager.hpp"
#include "rpc/Counters.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/handlers/AMMInfo.hpp"
#include "rpc/handlers/AccountChannels.hpp"
#include "rpc/handlers/AccountCurrencies.hpp"
#include "rpc/handlers/AccountInfo.hpp"
#include "rpc/handlers/AccountLines.hpp"
#include "rpc/handlers/AccountNFTs.hpp"
#include "rpc/handlers/AccountObjects.hpp"
#include "rpc/handlers/AccountOffers.hpp"
#include "rpc/handlers/AccountTx.hpp"
#include "rpc/handlers/BookChanges.hpp"
#include "rpc/handlers/BookOffers.hpp"
#include "rpc/handlers/DepositAuthorized.hpp"
#include "rpc/handlers/GatewayBalances.hpp"
#include "rpc/handlers/GetAggregatePrice.hpp"
#include "rpc/handlers/Ledger.hpp"
#include "rpc/handlers/LedgerData.hpp"
#include "rpc/handlers/LedgerEntry.hpp"
#include "rpc/handlers/LedgerRange.hpp"
#include "rpc/handlers/NFTBuyOffers.hpp"
#include "rpc/handlers/NFTHistory.hpp"
#include "rpc/handlers/NFTInfo.hpp"
#include "rpc/handlers/NFTSellOffers.hpp"
#include "rpc/handlers/NFTsByIssuer.hpp"
#include "rpc/handlers/NoRippleCheck.hpp"
#include "rpc/handlers/Ping.hpp"
#include "rpc/handlers/Random.hpp"
#include "rpc/handlers/ServerInfo.hpp"
#include "rpc/handlers/Subscribe.hpp"
#include "rpc/handlers/TransactionEntry.hpp"
#include "rpc/handlers/Tx.hpp"
#include "rpc/handlers/Unsubscribe.hpp"
#include "rpc/handlers/VersionHandler.hpp"
#include "util/config/Config.hpp"

#include <memory>
#include <optional>
#include <string>

namespace rpc::impl {

ProductionHandlerProvider::ProductionHandlerProvider(
    util::Config const& config,
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<feed::SubscriptionManager> const& subscriptionManager,
    std::shared_ptr<etl::LoadBalancer> const& balancer,
    std::shared_ptr<etl::ETLService const> const& etl,
    Counters const& counters
)
    : handlerMap_{
          {"account_channels", {AccountChannelsHandler{backend}}},
          {"account_currencies", {AccountCurrenciesHandler{backend}}},
          {"account_info", {AccountInfoHandler{backend}}},
          {"account_lines", {AccountLinesHandler{backend}}},
          {"account_nfts", {AccountNFTsHandler{backend}}},
          {"account_objects", {AccountObjectsHandler{backend}}},
          {"account_offers", {AccountOffersHandler{backend}}},
          {"account_tx", {AccountTxHandler{backend}}},
          {"amm_info", {AMMInfoHandler{backend}}},
          {"book_changes", {BookChangesHandler{backend}}},
          {"book_offers", {BookOffersHandler{backend}}},
          {"deposit_authorized", {DepositAuthorizedHandler{backend}}},
          {"gateway_balances", {GatewayBalancesHandler{backend}}},
          {"get_aggregate_price", {GetAggregatePriceHandler{backend}}},
          {"ledger", {LedgerHandler{backend}}},
          {"ledger_data", {LedgerDataHandler{backend}}},
          {"ledger_entry", {LedgerEntryHandler{backend}}},
          {"ledger_range", {LedgerRangeHandler{backend}}},
          {"nfts_by_issuer", {NFTsByIssuerHandler{backend}, true}},  // clio only
          {"nft_history", {NFTHistoryHandler{backend}, true}},       // clio only
          {"nft_buy_offers", {NFTBuyOffersHandler{backend}}},
          {"nft_info", {NFTInfoHandler{backend}, true}},  // clio only
          {"nft_sell_offers", {NFTSellOffersHandler{backend}}},
          {"noripple_check", {NoRippleCheckHandler{backend}}},
          {"ping", {PingHandler{}}},
          {"random", {RandomHandler{}}},
          {"server_info", {ServerInfoHandler{backend, subscriptionManager, balancer, etl, counters}}},
          {"transaction_entry", {TransactionEntryHandler{backend}}},
          {"tx", {TxHandler{backend, etl}}},
          {"subscribe", {SubscribeHandler{backend, subscriptionManager}}},
          {"unsubscribe", {UnsubscribeHandler{backend, subscriptionManager}}},
          {"version", {VersionHandler{config}}},
      }
{
}

bool
ProductionHandlerProvider::contains(std::string const& command) const
{
    return handlerMap_.contains(command);
}

std::optional<AnyHandler>
ProductionHandlerProvider::getHandler(std::string const& command) const
{
    if (!handlerMap_.contains(command))
        return {};

    return handlerMap_.at(command).handler;
}

bool
ProductionHandlerProvider::isClioOnly(std::string const& command) const
{
    return handlerMap_.contains(command) && handlerMap_.at(command).isClioOnly;
}

}  // namespace rpc::impl
