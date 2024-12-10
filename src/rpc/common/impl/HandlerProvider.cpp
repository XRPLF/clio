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

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "etl/ETLService.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
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
#include "rpc/handlers/Feature.hpp"
#include "rpc/handlers/GatewayBalances.hpp"
#include "rpc/handlers/GetAggregatePrice.hpp"
#include "rpc/handlers/Ledger.hpp"
#include "rpc/handlers/LedgerData.hpp"
#include "rpc/handlers/LedgerEntry.hpp"
#include "rpc/handlers/LedgerIndex.hpp"
#include "rpc/handlers/LedgerRange.hpp"
#include "rpc/handlers/MPTHolders.hpp"
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
    std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptionManager,
    std::shared_ptr<etl::LoadBalancer> const& balancer,
    std::shared_ptr<etl::ETLService const> const& etl,
    std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter,
    Counters const& counters
)
    : handlerMap_{
          {"account_channels", {.handler = AccountChannelsHandler{backend}}},
          {"account_currencies", {.handler = AccountCurrenciesHandler{backend}}},
          {"account_info", {.handler = AccountInfoHandler{backend, amendmentCenter}}},
          {"account_lines", {.handler = AccountLinesHandler{backend}}},
          {"account_nfts", {.handler = AccountNFTsHandler{backend}}},
          {"account_objects", {.handler = AccountObjectsHandler{backend}}},
          {"account_offers", {.handler = AccountOffersHandler{backend}}},
          {"account_tx", {.handler = AccountTxHandler{backend}}},
          {"amm_info", {.handler = AMMInfoHandler{backend}}},
          {"book_changes", {.handler = BookChangesHandler{backend}}},
          {"book_offers", {.handler = BookOffersHandler{backend}}},
          {"deposit_authorized", {.handler = DepositAuthorizedHandler{backend}}},
          {"feature", {.handler = FeatureHandler{backend, amendmentCenter}}},
          {"gateway_balances", {.handler = GatewayBalancesHandler{backend}}},
          {"get_aggregate_price", {.handler = GetAggregatePriceHandler{backend}}},
          {"ledger", {.handler = LedgerHandler{backend}}},
          {"ledger_data", {.handler = LedgerDataHandler{backend}}},
          {"ledger_entry", {.handler = LedgerEntryHandler{backend}}},
          {"ledger_index", {.handler = LedgerIndexHandler{backend}, .isClioOnly = true}},  // clio only
          {"ledger_range", {.handler = LedgerRangeHandler{backend}}},
          {"mpt_holders", {.handler = MPTHoldersHandler{backend}, .isClioOnly = true}},       // clio only
          {"nfts_by_issuer", {.handler = NFTsByIssuerHandler{backend}, .isClioOnly = true}},  // clio only
          {"nft_history", {.handler = NFTHistoryHandler{backend}, .isClioOnly = true}},       // clio only
          {"nft_buy_offers", {.handler = NFTBuyOffersHandler{backend}}},
          {"nft_info", {.handler = NFTInfoHandler{backend}, .isClioOnly = true}},  // clio only
          {"nft_sell_offers", {.handler = NFTSellOffersHandler{backend}}},
          {"noripple_check", {.handler = NoRippleCheckHandler{backend}}},
          {"ping", {.handler = PingHandler{}}},
          {"random", {.handler = RandomHandler{}}},
          {"server_info", {.handler = ServerInfoHandler{backend, subscriptionManager, balancer, etl, counters}}},
          {"transaction_entry", {.handler = TransactionEntryHandler{backend}}},
          {"tx", {.handler = TxHandler{backend, etl}}},
          {"subscribe", {.handler = SubscribeHandler{backend, subscriptionManager}}},
          {"unsubscribe", {.handler = UnsubscribeHandler{subscriptionManager}}},
          {"version", {.handler = VersionHandler{config}}},
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
