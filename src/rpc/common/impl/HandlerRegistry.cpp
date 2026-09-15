#include "rpc/common/impl/HandlerRegistry.hpp"

#include "rpc/Counters.hpp"  // IWYU pragma: keep
#include "rpc/common/AnyHandler.hpp"
#include "rpc/handlers/AMMInfo.hpp"
#include "rpc/handlers/AccountChannels.hpp"
#include "rpc/handlers/AccountCurrencies.hpp"
#include "rpc/handlers/AccountInfo.hpp"
#include "rpc/handlers/AccountLines.hpp"
#include "rpc/handlers/AccountMPTokenIssuances.hpp"
#include "rpc/handlers/AccountMPTokens.hpp"
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
#include "rpc/handlers/MPTokenIssuanceHistory.hpp"
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
#include "rpc/handlers/VaultInfo.hpp"
#include "rpc/handlers/VersionHandler.hpp"
#include "util/Concepts.hpp"

#include <array>
#include <span>
#include <tuple>

namespace rpc::impl {

namespace {

constexpr auto kHandlers = std::to_array<HandlerEntry>({
    {
        .name = "account_channels",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountChannelsHandler{d.backend};
        },
    },

    {
        .name = "account_currencies",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountCurrenciesHandler{d.backend};
        },
    },

    {
        .name = "account_info",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountInfoHandler{d.backend, d.amendmentCenter};
        },
    },

    {
        .name = "account_lines",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountLinesHandler{d.backend};
        },
    },

    {
        .name = "account_mptoken_issuances",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountMPTokenIssuancesHandler{d.backend};
        },
        .isClioOnly = true,
    },

    {
        .name = "account_mptokens",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountMPTokensHandler{d.backend};
        },
        .isClioOnly = true,
    },

    {
        .name = "account_nfts",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return AccountNFTsHandler{d.backend}; },
    },

    {
        .name = "account_objects",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountObjectsHandler{d.backend};
        },
    },

    {
        .name = "account_offers",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountOffersHandler{d.backend};
        },
    },

    {
        .name = "account_tx",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AccountTxHandler{d.backend, d.etl};
        },
    },

    {
        .name = "amm_info",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return AMMInfoHandler{d.backend, d.amendmentCenter};
        },
    },

    {
        .name = "book_changes",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return BookChangesHandler{d.backend}; },
    },

    {
        .name = "book_offers",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return BookOffersHandler{d.backend, d.amendmentCenter};
        },
    },

    {
        .name = "deposit_authorized",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return DepositAuthorizedHandler{d.backend};
        },
    },

    {
        .name = "feature",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return FeatureHandler{d.backend, d.amendmentCenter};
        },
    },

    {
        .name = "gateway_balances",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return GatewayBalancesHandler{d.backend};
        },
    },

    {
        .name = "get_aggregate_price",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return GetAggregatePriceHandler{d.backend};
        },
    },

    {
        .name = "ledger",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return LedgerHandler{d.backend, d.amendmentCenter};
        },
    },

    {
        .name = "ledger_data",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return LedgerDataHandler{d.backend}; },
    },

    {
        .name = "ledger_entry",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return LedgerEntryHandler{d.backend}; },
    },

    {
        .name = "ledger_index",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return LedgerIndexHandler{d.backend}; },
        .isClioOnly = true,
    },

    {
        .name = "ledger_range",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return LedgerRangeHandler{d.backend}; },
    },

    {
        .name = "mpt_holders",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return MPTHoldersHandler{d.backend}; },
        .isClioOnly = true,
    },

    {
        .name = "mptoken_issuance_history",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return MPTokenIssuanceHistoryHandler{d.backend};
        },
        .isClioOnly = true,
    },

    {
        .name = "nfts_by_issuer",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return NFTsByIssuerHandler{d.backend};
        },
        .isClioOnly = true,
    },

    {
        .name = "nft_history",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return NFTHistoryHandler{d.backend}; },
        .isClioOnly = true,
    },

    {
        .name = "nft_buy_offers",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return NFTBuyOffersHandler{d.backend};
        },
    },

    {
        .name = "nft_info",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return NFTInfoHandler{d.backend}; },
        .isClioOnly = true,
    },

    {
        .name = "nft_sell_offers",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return NFTSellOffersHandler{d.backend};
        },
    },

    {
        .name = "noripple_check",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return NoRippleCheckHandler{d.backend};
        },
    },

    {
        .name = "ping",
        .factory = [](HandlerDeps const&) -> AnyHandler { return PingHandler{}; },
    },

    {
        .name = "random",
        .factory = [](HandlerDeps const&) -> AnyHandler { return RandomHandler{}; },
    },

    {
        .name = "server_info",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return ServerInfoHandler{
                d.backend, d.subscriptionManager, d.balancer, d.etl, d.counters
            };
        },
    },

    {
        .name = "transaction_entry",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return TransactionEntryHandler{d.backend};
        },
    },

    {
        .name = "tx",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return TxHandler{d.backend, d.etl}; },
    },

    {
        .name = "subscribe",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return SubscribeHandler{d.backend, d.amendmentCenter, d.subscriptionManager};
        },
    },

    {
        .name = "unsubscribe",
        .factory = [](HandlerDeps const& d) -> AnyHandler {
            return UnsubscribeHandler{d.subscriptionManager};
        },
    },

    {
        .name = "vault_info",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return VaultInfoHandler{d.backend}; },
    },

    {
        .name = "version",
        .factory = [](HandlerDeps const& d) -> AnyHandler { return VersionHandler{d.config}; },
    },
});

// A duplicate name would silently shadow a handler so we check at compile time.
static_assert(
    std::apply(
        [](auto const&... entry) { return util::hasNoDuplicates(entry.name...); },
        kHandlers
    ),
    "RPC handler names must be unique"
);

}  // namespace

std::span<HandlerEntry const>
handlerRegistry() noexcept
{
    return kHandlers;
}

}  // namespace rpc::impl
