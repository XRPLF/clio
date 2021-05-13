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
#include <reporting/BackendInterface.h>

// {
//   account: account,
//   ledger_index_min: ledger_index  // optional, defaults to earliest
//   ledger_index_max: ledger_index, // optional, defaults to latest
//   binary: boolean,                // optional, defaults to false
//   forward: boolean,               // optional, defaults to false
//   limit: integer,                 // optional
//   marker: object {ledger: ledger_index, seq: txn_sequence} // optional,
//   resume previous query
// }
boost::json::object
doAccountTx(boost::json::object const& request, BackendInterface const& backend)
{
    boost::json::object response;

    if (!request.contains("account"))
    {
        response["error"] = "Please specify an account";
        return response;
    }

    auto account = ripple::parseBase58<ripple::AccountID>(
        request.at("account").as_string().c_str());
    if (!account)
    {
        account = ripple::AccountID();
        if (!account->parseHex(request.at("account").as_string().c_str()))
        {
            response["error"] = "account malformed";
            return response;
        }
    }
    auto ledgerSequence = ledgerSequenceFromRequest(request, backend);
    if (!ledgerSequence)
    {
        response["error"] = "Empty database";
        return response;
    }
    bool binary =
        request.contains("binary") ? request.at("binary").as_bool() : false;

    std::optional<Backend::AccountTransactionsCursor> cursor;
    if (request.contains("cursor"))
    {
        auto const& obj = request.at("cursor").as_object();
        std::optional<uint32_t> ledgerSequence;
        if (obj.contains("ledger_sequence"))
        {
            ledgerSequence = (uint32_t)obj.at("ledger_sequence").as_int64();
        }
        std::optional<uint32_t> transactionIndex;
        if (obj.contains("transaction_index"))
        {
            transactionIndex = (uint32_t)obj.at("transaction_index").as_int64();
        }
        if (!ledgerSequence || !transactionIndex)
        {
            response["error"] =
                "malformed cursor. include transaction_index and "
                "ledger_sequence in an object named \"cursor\"";
            return response;
        }
        cursor = {*ledgerSequence, *transactionIndex};
    }

    uint32_t limit = 200;
    if (request.contains("limit") and
        request.at("limit").kind() == boost::json::kind::int64)
        limit = request.at("limit").as_int64();
    boost::json::array txns;
    auto start = std::chrono::system_clock::now();
    auto [blobs, retCursor] =
        backend.fetchAccountTransactions(*account, limit, cursor);
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " db fetch took "
                            << ((end - start).count() / 1000000000.0)
                            << " num blobs = " << blobs.size();
    for (auto const& txnPlusMeta : blobs)
    {
        if (txnPlusMeta.ledgerSequence > ledgerSequence)
        {
            BOOST_LOG_TRIVIAL(debug)
                << __func__
                << " skipping over transactions from incomplete ledger";
            continue;
        }
        boost::json::object obj;
        if (!binary)
        {
            auto [txn, meta] = deserializeTxPlusMeta(txnPlusMeta);
            obj["transaction"] = getJson(*txn);
            obj["metadata"] = getJson(*meta);
        }
        else
        {
            obj["transaction"] = ripple::strHex(txnPlusMeta.transaction);
            obj["metadata"] = ripple::strHex(txnPlusMeta.metadata);
        }
        obj["ledger_sequence"] = txnPlusMeta.ledgerSequence;
        txns.push_back(obj);
    }
    response["transactions"] = txns;
    if (retCursor)
    {
        boost::json::object cursorJson;
        cursorJson["ledger_sequence"] = retCursor->ledgerSequence;
        cursorJson["transaction_index"] = retCursor->transactionIndex;
        response["cursor"] = cursorJson;
    }
    auto end2 = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " serialization took "
                            << ((end2 - end).count() / 1000000000.0);
    return response;
}

