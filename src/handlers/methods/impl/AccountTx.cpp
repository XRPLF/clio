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
#include <handlers/RPCHelpers.h>
#include <handlers/methods/Transaction.h>

namespace RPC {

Result
doAccountTx(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    if (!request.contains("account"))
        return Status{Error::rpcINVALID_PARAMS, "missingAccount"};

    if (!request.at("account").is_string())
        return Status{Error::rpcINVALID_PARAMS, "accountNotString"};

    auto accountID =
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};

    bool binary = false;
    if (request.contains("binary"))
    {
        if (!request.at("binary").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = request.at("binary").as_bool();
    }

    auto minIndex = context.range.minSequence;
    if (request.contains("ledger_index_min"))
    {
        if (!request.at("ledger_index_min").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "ledgerSeqMinNotNumber"};

        minIndex = value_to<std::uint32_t>(request.at("ledger_index_min"));
    }

    std::optional<Backend::AccountTransactionsCursor> cursor;
    cursor = {context.range.maxSequence, 0};

    if (request.contains("marker"))
    {
        auto const& obj = request.at("marker").as_object();

        std::optional<std::uint32_t> transactionIndex = {};
        if (obj.contains("seq"))
        {
            if (!obj.at("seq").is_int64())
                return Status{
                    Error::rpcINVALID_PARAMS, "transactionIndexNotInt"};

            transactionIndex = value_to<std::uint32_t>(obj.at("seq"));
        }

        std::optional<std::uint32_t> ledgerIndex = {};
        if (obj.contains("ledger"))
        {
            if (!obj.at("ledger").is_int64())
                return Status{Error::rpcINVALID_PARAMS, "ledgerIndexNotInt"};

            ledgerIndex = value_to<std::uint32_t>(obj.at("ledger"));
        }

        if (!transactionIndex || !ledgerIndex)
            return Status{Error::rpcINVALID_PARAMS, "missingLedgerOrSeq"};

        cursor = {*ledgerIndex, *transactionIndex};
    }
    else if (request.contains("ledger_index_max"))
    {
        if (!request.at("ledger_index_max").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "ledgerSeqMaxNotNumber"};

        auto maxIndex = value_to<std::uint32_t>(request.at("ledger_index_max"));

        if (minIndex > maxIndex)
            return Status{Error::rpcINVALID_PARAMS, "invalidIndex"};

        cursor = {maxIndex, 0};
    }

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if (!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};

        response["limit"] = limit;
    }

    boost::json::array txns;
    auto start = std::chrono::system_clock::now();
    auto [blobs, retCursor] =
        context.backend->fetchAccountTransactions(*accountID, limit, cursor);

    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " db fetch took "
                            << ((end - start).count() / 1000000000.0)
                            << " num blobs = " << blobs.size();

    response["account"] = ripple::to_string(*accountID);
    response["ledger_index_min"] = minIndex;
    response["ledger_index_max"] = cursor->ledgerSequence;

    if (retCursor)
    {
        BOOST_LOG_TRIVIAL(debug) << "setting json cursor";
        boost::json::object cursorJson;
        cursorJson["ledger"] = retCursor->ledgerSequence;
        cursorJson["seq"] = retCursor->transactionIndex;
        response["marker"] = cursorJson;
    }

    for (auto const& txnPlusMeta : blobs)
    {
        if (txnPlusMeta.ledgerSequence < minIndex)
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
            obj["meta"] = toJson(*meta);
            obj["tx"] = toJson(*txn);
            obj["tx"].as_object()["ledger_index"] = txnPlusMeta.ledgerSequence;
            obj["tx"].as_object()["inLedger"] = txnPlusMeta.ledgerSequence;
        }
        else
        {
            obj["meta"] = ripple::strHex(txnPlusMeta.metadata);
            obj["tx_blob"] = ripple::strHex(txnPlusMeta.transaction);
            obj["ledger_index"] = txnPlusMeta.ledgerSequence;
        }

        obj["validated"] = true;

        txns.push_back(obj);
    }

    response["transactions"] = txns;

    auto end2 = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " serialization took "
                            << ((end2 - end).count() / 1000000000.0);

    return response;
}

}  // namespace RPC
