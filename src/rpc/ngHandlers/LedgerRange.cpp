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

#include <rpc/RPCHelpers.h>
#include <rpc/ngHandlers/LedgerRange.h>

#include <optional>

namespace RPCng {

LedgerRangeHandler::Result
LedgerRangeHandler::process() const
{
    if (auto const maybeRange = sharedPtrBackend_->fetchLedgerRange();
        maybeRange)
    {
        return Output{*maybeRange};
    }
    else
    {
        return Error{
            RPC::Status{RPC::RippledError::rpcNOT_READY, "rangeNotFound"}};
    }
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    LedgerRangeHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(ledger_index_min), output.range.minSequence},
        {JS(ledger_index_max), output.range.maxSequence},
    };
}

}  // namespace RPCng
