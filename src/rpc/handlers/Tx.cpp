//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <backend/BackendInterface.h>
#include <backend/Pg.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

// {
//   transaction: <hex>
// }

Result
doTx(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    if (!request.contains("transaction"))
        return Status{Error::rpcINVALID_PARAMS, "specifyTransaction"};

    if (!request.at("transaction").is_string())
        return Status{Error::rpcINVALID_PARAMS, "transactionNotString"};

    ripple::uint256 hash;
    if (!hash.parseHex(request.at("transaction").as_string().c_str()))
        return Status{Error::rpcINVALID_PARAMS, "malformedTransaction"};

    bool binary = false;
    if (request.contains("binary"))
    {
        if (!request.at("binary").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = request.at("binary").as_bool();
    }

    auto range = context.backend->fetchLedgerRange();
    if (!range)
        return Status{Error::rpcNOT_READY};

    auto dbResponse = context.backend->fetchTransaction(hash);
    if (!dbResponse)
        return Status{Error::rpcTXN_NOT_FOUND};

    auto lgrInfo =
        context.backend->fetchLedgerBySequence(dbResponse->ledgerSequence);
    if (!binary)
    {
        auto [txn, meta] = toExpandedJson(*dbResponse);
        response = txn;
        response["meta"] = meta;
    }
    else
    {
        response["tx"] = ripple::strHex(dbResponse->transaction);
        response["meta"] = ripple::strHex(dbResponse->metadata);
        response["hash"] = request.at("transaction").as_string();
    }
    response["date"] = lgrInfo->closeTime.time_since_epoch().count();
    response["ledger_index"] = dbResponse->ledgerSequence;

    return response;
}

}  // namespace RPC
