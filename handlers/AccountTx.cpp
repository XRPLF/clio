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

std::vector<std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::STObject const>>>
doAccountTxStoredProcedure(
    ripple::AccountID const& account,
    std::shared_ptr<PgPool>& pgPool,
    CassandraFlatMapBackend const& backend)
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
boost::json::object
doAccountTx(
    boost::json::object const& request,
    CassandraFlatMapBackend const& backend,
    std::shared_ptr<PgPool>& pgPool)
{
    boost::json::object response;

    if (!request.contains("account"))
    {
        response["error"] = "Please specify an account";
        return response;
    }

    auto const account = ripple::parseBase58<ripple::AccountID>(
        request.at("account").as_string().c_str());
    if (!account)
    {
        response["error"] = "account malformed";
        return response;
    }

    boost::json::array txns;
    auto res = doAccountTxStoredProcedure(*account, pgPool, backend);
    for (auto const& [sttx, meta] : res)
    {
        boost::json::object obj;
        obj["transaction"] = getJson(*sttx);
        obj["metadata"] = getJson(*meta);
        txns.push_back(obj);
    }
    response["transactions"] = txns;
    return response;
}

