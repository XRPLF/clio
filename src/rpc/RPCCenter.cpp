#include "rpc/RPCCenter.hpp"

#include <string_view>
#include <unordered_set>

namespace rpc {

namespace {

std::unordered_set<std::string_view> const&
handledRpcs()
{
    static std::unordered_set<std::string_view> const kHandledRpcs = {
        // clang-format off
        "account_channels",
        "account_currencies",
        "account_info",
        "account_lines",
        "account_mptoken_issuances",
        "account_mptokens",
        "account_nfts",
        "account_objects",
        "account_offers",
        "account_tx",
        "amm_info",
        "book_changes",
        "book_offers",
        "deposit_authorized",
        "feature",
        "gateway_balances",
        "get_aggregate_price",
        "ledger",
        "ledger_data",
        "ledger_entry",
        "ledger_index",
        "ledger_range",
        "mpt_holders",
        "nfts_by_issuer",
        "nft_history",
        "nft_buy_offers",
        "nft_info",
        "nft_sell_offers",
        "noripple_check",
        "ping",
        "random",
        "server_info",
        "transaction_entry",
        "tx",
        "subscribe",
        "unsubscribe",
        "vault_info",
        "version",
        // clang-format on
    };
    return kHandledRpcs;
}

std::unordered_set<std::string_view> const&
forwardedRpcs()
{
    static std::unordered_set<std::string_view> const kForwardedRpcs = {
        "server_definitions",
        "server_state",
        "submit",
        "submit_multisigned",
        "fee",
        "ledger_closed",
        "ledger_current",
        "ripple_path_find",
        "manifest",
        "channel_authorize",
        "channel_verify",
        "simulate",
        "batch"
    };
    return kForwardedRpcs;
}

}  // namespace

bool
RPCCenter::isRpcName(std::string_view s)
{
    return isHandled(s) || isForwarded(s);
}

bool
RPCCenter::isHandled(std::string_view s)
{
    return handledRpcs().contains(s);
}

bool
RPCCenter::isForwarded(std::string_view s)
{
    return forwardedRpcs().contains(s);
}

}  // namespace rpc
