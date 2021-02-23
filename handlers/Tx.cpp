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

#include <handlers/RPCHelpers.h>
#include <reporting/Pg.h>
#include <reporting/ReportingBackend.h>

// {
//   transaction: <hex>
// }

boost::json::object
doTx(
    boost::json::object const& request,
    CassandraFlatMapBackend const& backend,
    std::shared_ptr<PgPool>& postgres)
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

    auto range = getLedgerRange(postgres);
    if (!range)
    {
        response["error"] = "Database is empty";
        return response;
    }

    auto dbResponse = backend.fetchTransaction(hash);
    if (!dbResponse)
    {
        response["error"] = "Transaction not found in Cassandra";
        response["ledger_range"] = std::to_string(range->lower()) + " - " +
            std::to_string(range->upper());

        return response;
    }

    auto [sttx, meta] = deserializeTxPlusMeta(dbResponse.value());
    response["transaction"] = getJson(*sttx);
    response["meta"] = getJson(*meta);
    return response;
}

