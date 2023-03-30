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

#include <rpc/BookChanges.h>

#include <boost/json.hpp>

namespace json = boost::json;
using namespace ripple;

namespace RPC {

void
tag_invoke(json::value_from_tag, json::value& jv, BookChange const& change)
{
    auto amountStr = [](STAmount const& amount) -> std::string {
        return isXRP(amount) ? to_string(amount.xrp())
                             : to_string(amount.iou());
    };

    auto currencyStr = [](STAmount const& amount) -> std::string {
        return isXRP(amount) ? "XRP_drops" : to_string(amount.issue());
    };

    jv = {
        {JS(currency_a), currencyStr(change.sideAVolume)},
        {JS(currency_b), currencyStr(change.sideBVolume)},
        {JS(volume_a), amountStr(change.sideAVolume)},
        {JS(volume_b), amountStr(change.sideBVolume)},
        {JS(high), to_string(change.highRate.iou())},
        {JS(low), to_string(change.lowRate.iou())},
        {JS(open), to_string(change.openRate.iou())},
        {JS(close), to_string(change.closeRate.iou())},
    };
}

json::object const
computeBookChanges(
    ripple::LedgerInfo const& lgrInfo,
    std::vector<Backend::TransactionAndMetadata> const& transactions)
{
    return {
        {JS(type), "bookChanges"},
        {JS(ledger_index), lgrInfo.seq},
        {JS(ledger_hash), to_string(lgrInfo.hash)},
        {JS(ledger_time), lgrInfo.closeTime.time_since_epoch().count()},
        {JS(changes), json::value_from(BookChanges::compute(transactions))},
    };
}

}  // namespace RPC
