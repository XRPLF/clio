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

#include <rpc/common/impl/HandlerProvider.h>

#include <etl/ETLService.h>
#include <rpc/Counters.h>
#include <subscriptions/SubscriptionManager.h>

#include <rpc/handlers/AccountChannels.h>
#include <rpc/handlers/AccountCurrencies.h>
#include <rpc/handlers/AccountInfo.h>
#include <rpc/handlers/AccountLines.h>
#include <rpc/handlers/AccountNFTs.h>
#include <rpc/handlers/AccountObjects.h>
#include <rpc/handlers/AccountOffers.h>
#include <rpc/handlers/AccountTx.h>
#include <rpc/handlers/BookChanges.h>
#include <rpc/handlers/BookOffers.h>
#include <rpc/handlers/DepositAuthorized.h>
#include <rpc/handlers/GatewayBalances.h>
#include <rpc/handlers/Ledger.h>
#include <rpc/handlers/LedgerData.h>
#include <rpc/handlers/LedgerEntry.h>
#include <rpc/handlers/LedgerRange.h>
#include <rpc/handlers/NFTBuyOffers.h>
#include <rpc/handlers/NFTHistory.h>
#include <rpc/handlers/NFTInfo.h>
#include <rpc/handlers/NFTSellOffers.h>
#include <rpc/handlers/NoRippleCheck.h>
#include <rpc/handlers/Ping.h>
#include <rpc/handlers/Random.h>
#include <rpc/handlers/ServerInfo.h>
#include <rpc/handlers/Subscribe.h>
#include <rpc/handlers/TransactionEntry.h>
#include <rpc/handlers/Tx.h>
#include <rpc/handlers/Unsubscribe.h>

namespace RPC::detail {

ProductionHandlerProvider::ProductionHandlerProvider(
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptionManager,
    std::shared_ptr<LoadBalancer> const& balancer,
    std::shared_ptr<ETLService const> const& etl,
    Counters const& counters)
    : handlerMap_{
          {"account_channels", {AccountChannelsHandler{backend}}},
          {"account_currencies", {AccountCurrenciesHandler{backend}}},
          {"account_info", {AccountInfoHandler{backend}}},
          {"account_lines", {AccountLinesHandler{backend}}},
          {"account_nfts", {AccountNFTsHandler{backend}}},
          {"account_objects", {AccountObjectsHandler{backend}}},
          {"account_offers", {AccountOffersHandler{backend}}},
          {"account_tx", {AccountTxHandler{backend}}},
          {"book_changes", {BookChangesHandler{backend}}},
          {"book_offers", {BookOffersHandler{backend}}},
          {"deposit_authorized", {DepositAuthorizedHandler{backend}}},
          {"gateway_balances", {GatewayBalancesHandler{backend}}},
          {"ledger", {LedgerHandler{backend}}},
          {"ledger_data", {LedgerDataHandler{backend}}},
          {"ledger_entry", {LedgerEntryHandler{backend}}},
          {"ledger_range", {LedgerRangeHandler{backend}}},
          {"nft_history", {NFTHistoryHandler{backend}, true}},  // clio only
          {"nft_buy_offers", {NFTBuyOffersHandler{backend}}},
          {"nft_info", {NFTInfoHandler{backend}, true}},  // clio only
          {"nft_sell_offers", {NFTSellOffersHandler{backend}}},
          {"noripple_check", {NoRippleCheckHandler{backend}}},
          {"ping", {PingHandler{}}},
          {"random", {RandomHandler{}}},
          {"server_info", {ServerInfoHandler{backend, subscriptionManager, balancer, etl, counters}}},
          {"transaction_entry", {TransactionEntryHandler{backend}}},
          {"tx", {TxHandler{backend}}},
          {"subscribe", {SubscribeHandler{backend, subscriptionManager}}},
          {"unsubscribe", {UnsubscribeHandler{backend, subscriptionManager}}},
      }
{
}

bool
ProductionHandlerProvider::contains(std::string const& method) const
{
    return handlerMap_.contains(method);
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

}  // namespace RPC::detail
