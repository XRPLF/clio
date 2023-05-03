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

#include <etl/ReportingETL.h>
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
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    std::shared_ptr<ReportingETL const> const& etl,
    Counters const& counters)
    : handlerMap_{
          {"account_channels", {AnyHandler{AccountChannelsHandler{backend}}}},
          {"account_currencies", {AnyHandler{AccountCurrenciesHandler{backend}}}},
          {"account_info", {AnyHandler{AccountInfoHandler{backend}}}},
          {"account_lines", {AnyHandler{AccountLinesHandler{backend}}}},
          {"account_nfts", {AnyHandler{AccountNFTsHandler{backend}}}},
          {"account_objects", {AnyHandler{AccountObjectsHandler{backend}}}},
          {"account_offers", {AnyHandler{AccountOffersHandler{backend}}}},
          {"account_tx", {AnyHandler{AccountTxHandler{backend}}}},
          {"book_changes", {AnyHandler{BookChangesHandler{backend}}}},
          {"book_offers", {AnyHandler{BookOffersHandler{backend}}}},
          {"gateway_balances", {AnyHandler{GatewayBalancesHandler{backend}}}},
          {"ledger", {AnyHandler{LedgerHandler{backend}}}},
          {"ledger_data", {AnyHandler{LedgerDataHandler{backend}}}},
          {"ledger_entry", {AnyHandler{LedgerEntryHandler{backend}}}},
          {"ledger_range", {AnyHandler{LedgerRangeHandler{backend}}}},
          {"nft_buy_offers", {AnyHandler{NFTBuyOffersHandler{backend}}}},
          {"nft_info", {AnyHandler{NFTInfoHandler{backend}}}},
          {"nft_sell_offers", {AnyHandler{NFTSellOffersHandler{backend}}}},
          {"noripple_check", {AnyHandler{NoRippleCheckHandler{backend}}}},
          {"ping", {AnyHandler{PingHandler{}}}},
          {"random", {AnyHandler{RandomHandler{}}}},
          {"server_info", {AnyHandler{ServerInfoHandler{backend, subscriptionManager, balancer, etl, counters}}}},
          {"transaction_entry", {AnyHandler{TransactionEntryHandler{backend}}}},
          {"tx", {AnyHandler{TxHandler{backend}}}},
          {"unsubscribe", {AnyHandler{UnsubscribeHandler{backend, subscriptionManager}}}},
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
