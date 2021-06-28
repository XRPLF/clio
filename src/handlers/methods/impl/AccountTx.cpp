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
#include <handlers/methods/Transaction.h>
#include <backend/BackendInterface.h>
#include <backend/Pg.h>

namespace RPC
{

std::vector<std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::STObject const>>>
doAccountTxStoredProcedure(
    ripple::AccountID const& account,
    std::shared_ptr<PgPool>& pgPool,
    BackendInterface const& backend)
{
    pg_params dbParams;

    char const*& command = dbParams.first;
    std::vector<std::optional<std::string>>& values = dbParams.second;
    command =
        "SELECT account_tx($1::bytea, $2::bool, "
        "$3::bigint, $4::bigint, $5::bigint, $6::bytea, "
        "$7::bigint, $8::bool, $9::bigint, $10::bigint)";
    values.resize(10);
    values[0] = "\\x" + ripple::strHex(account);
    values[1] = "true";
    static std::uint32_t const page_length(200);
    values[2] = std::to_string(page_length);

    auto res = PgQuery(pgPool)(dbParams);
    if (!res)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " : Postgres response is null - account = "
            << ripple::strHex(account);
        assert(false);
        return {};
    }
    else if (res.status() != PGRES_TUPLES_OK)
    {
        assert(false);
        return {};
    }

    if (res.isNull() || res.ntuples() == 0)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " : No data returned from Postgres : account = "
            << ripple::strHex(account);

        assert(false);
        return {};
    }

    char const* resultStr = res.c_str();

    boost::json::object result = boost::json::parse(resultStr).as_object();
    if (result.contains("transactions"))
    {
        std::vector<ripple::uint256> nodestoreHashes;
        for (auto& t : result.at("transactions").as_array())
        {
            boost::json::object obj = t.as_object();
            if (obj.contains("ledger_seq") && obj.contains("nodestore_hash"))
            {
                std::string nodestoreHashHex =
                    obj.at("nodestore_hash").as_string().c_str();
                nodestoreHashHex.erase(0, 2);
                ripple::uint256 nodestoreHash;
                if (!nodestoreHash.parseHex(nodestoreHashHex))
                    assert(false);

                if (nodestoreHash.isNonZero())
                {
                    nodestoreHashes.push_back(nodestoreHash);
                }
                else
                {
                    assert(false);
                }
            }
            else
            {
                assert(false);
            }
        }

        std::vector<std::pair<
            std::shared_ptr<ripple::STTx const>,
            std::shared_ptr<ripple::STObject const>>>
            results;
        auto dbResults = backend.fetchTransactions(nodestoreHashes);
        for (auto const& res : dbResults)
        {
            if (res.transaction.size() && res.metadata.size())
                results.push_back(deserializeTxPlusMeta(res));
        }
        return results;
    }
    return {};
}

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
Status
AccountTx::check()
{
    auto request = context_.params;

    if(!request.contains("account"))
        return {Error::rpcINVALID_PARAMS, "missingAccount"};

    if(!request.at("account").is_string())
        return {Error::rpcINVALID_PARAMS, "accountNotString"};
    
    auto accountID = 
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return {Error::rpcINVALID_PARAMS, "malformedAccount"};

    bool binary = false;
    if(request.contains("binary"))
    {
        if(!request.at("binary").is_bool())
            return {Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};
        
        binary = request.at("binary").as_bool();
    }

    auto minIndex = context_.range.minSequence;
    if (request.contains("ledger_index_min"))
    {
        if (!request.at("ledger_index_min").is_int64())
            return {Error::rpcINVALID_PARAMS, "ledgerSeqMinNotNumber"};

        minIndex = value_to<std::uint32_t>(request.at("ledger_index_min"));
    }

    std::optional<Backend::AccountTransactionsCursor> cursor;
    cursor = {context_.range.maxSequence, 0};

    if (request.contains("cursor"))
    {
        auto const& obj = request.at("cursor").as_object();

        std::optional<std::uint32_t> transactionIndex = {};
        if (obj.contains("seq"))
        {
            if (!obj.at("seq").is_int64())
                return {Error::rpcINVALID_PARAMS, "transactionIndexNotInt"};

            transactionIndex = value_to<std::uint32_t>(obj.at("seq"));
        }

        std::optional<std::uint32_t> ledgerIndex = {};
        if (obj.contains("ledger"))
        {
            if (!obj.at("ledger").is_int64())
                return {Error::rpcINVALID_PARAMS, "transactionIndexNotInt"};

            transactionIndex = value_to<std::uint32_t>(obj.at("ledger"));
        }

        if (!transactionIndex || !ledgerIndex)
            return {Error::rpcINVALID_PARAMS, "missingLedgerOrSeq"};

        cursor = {*ledgerIndex, *transactionIndex};
    }
    else if (request.contains("ledger_index_max"))
    {
        if (!request.at("ledger_index_max").is_int64())
            return {Error::rpcINVALID_PARAMS, "ledgerSeqMaxNotNumber"};

        auto maxIndex = value_to<std::uint32_t>(request.at("ledger_index_max"));

        if (minIndex > maxIndex)
            return {Error::rpcINVALID_PARAMS, "invalidIndex"};

        cursor = {maxIndex, 0};
    }

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if(!request.at("limit").is_int64())
            return {Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return {Error::rpcINVALID_PARAMS, "limitNotPositive"};

        response_["limit"] = limit;
    }


    boost::json::array txns;
    auto start = std::chrono::system_clock::now();
    auto [blobs, retCursor] =
        context_.backend->fetchAccountTransactions(*accountID, limit, cursor);

    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " db fetch took " << ((end - start).count() / 1000000000.0) << " num blobs = " << blobs.size();

    response_["account"] = ripple::to_string(*accountID);
    response_["ledger_index_min"] = minIndex;
    response_["ledger_index_max"] = cursor->ledgerSequence;

    if (retCursor)
    {
        boost::json::object cursorJson;
        cursorJson["ledger"] = retCursor->ledgerSequence;
        cursorJson["seq"] = retCursor->transactionIndex;
        response_["marker"] = cursorJson;
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

    response_["transactions"] = txns;

    auto end2 = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " serialization took " << ((end2 - end).count() / 1000000000.0);
    
    return OK;
}

} // namespace RPC