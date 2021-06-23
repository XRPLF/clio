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
#include <handlers/RPCHelpers.h>

// {
//   transaction: <hex>
// }

boost::json::object
doTx(boost::json::object const& request, BackendInterface const& backend)
{
    boost::json::object response;
    if (!request.contains("transaction"))
    {
        response["error"] = "Please specify a transaction hash";
        return response;
    }
    ripple::uint256 hash;
    if (!hash.parseHex(request.at("transaction").as_string().c_str()))
    {
        response["error"] = "Error parsing transaction hash";
        return response;
    }

    auto range = backend.fetchLedgerRange();
    if (!range)
    {
        response["error"] = "Database is empty";
        return response;
    }

    auto dbResponse = backend.fetchTransaction(hash);
    if (!dbResponse)
    {
        response["error"] = "Transaction not found in Cassandra";
        response["ledger_range"] = std::to_string(range->minSequence) + " - " +
            std::to_string(range->maxSequence);

        return response;
    }

    bool binary =
        request.contains("binary") ? request.at("binary").as_bool() : false;
    if (!binary)
    {
        auto [sttx, meta] = deserializeTxPlusMeta(dbResponse.value());
        response["transaction"] = toJson(*sttx);
        response["metadata"] = toJson(*meta);
    }
    else
    {
        response["transaction"] = ripple::strHex(dbResponse->transaction);
        response["metadata"] = ripple::strHex(dbResponse->metadata);
    }
    response["ledger_sequence"] = dbResponse->ledgerSequence;
    return response;
}

