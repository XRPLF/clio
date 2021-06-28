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

#include <rpc/RPCHelpers.h>
#include <rpc/handlers/Transaction.h>
#include <reporting/BackendInterface.h>
#include <reporting/Pg.h>

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

    auto v = ledgerInfoFromRequest(context_);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

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

    std::optional<Backend::AccountTransactionsCursor> cursor;
    if (request.contains("cursor"))
    {
        auto const& obj = request.at("cursor").as_object();
        std::optional<uint32_t> transactionIndex;
        if (obj.contains("transaction_index"))
        {
            if (!obj.at("transaction_index").is_int64())
                return {Error::rpcINVALID_PARAMS, "transactionIndexNotInt"};

            transactionIndex = (uint32_t)obj.at("transaction_index").as_int64();
        }

        cursor = {lgrInfo.seq, *transactionIndex};
    }

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if(!request.at("limit").is_int64())
            return {Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return {Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }


    boost::json::array txns;
    auto start = std::chrono::system_clock::now();
    auto [blobs, retCursor] =
        context_.backend->fetchAccountTransactions(*accountID, limit, cursor);
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " db fetch took " << ((end - start).count() / 1000000000.0) << " num blobs = " << blobs.size();
    
    for (auto const& txnPlusMeta : blobs)
    {
        if (txnPlusMeta.ledgerSequence > lgrInfo.seq)
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

    response_["transactions"] = txns;
    if (retCursor)
    {
        boost::json::object cursorJson;
        cursorJson["ledger_sequence"] = retCursor->ledgerSequence;
        cursorJson["transaction_index"] = retCursor->transactionIndex;
        response_["cursor"] = cursorJson;
    }

    auto end2 = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " serialization took " << ((end2 - end).count() / 1000000000.0);
    
    return OK;
}

} // namespace RPC