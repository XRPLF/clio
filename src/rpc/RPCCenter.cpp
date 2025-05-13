//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "rpc/RPCCenter.hpp"

#include <string_view>
#include <unordered_set>

namespace rpc {

namespace {

std::unordered_set<std::string_view> const&
handledRpcs()
{
    static std::unordered_set<std::string_view> kHANDLED_RPCS = {
        "account_channels",
        "account_currencies",
        "account_info",
        "account_lines",
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
        "version",
    };
    return kHANDLED_RPCS;
}

std::unordered_set<std::string_view> const&
forwardedRpcs()
{
    static std::unordered_set<std::string_view> const kFORWARDED_RPCS = {
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
    };
    return kFORWARDED_RPCS;
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
