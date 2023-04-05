//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doLedger(Context const& context)
{
    auto params = context.params;
    boost::json::object response = {};

    bool binary = false;
    if (params.contains(JS(binary)))
    {
        if (!params.at(JS(binary)).is_bool())
            return Status{RippledError::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = params.at(JS(binary)).as_bool();
    }

    bool transactions = false;
    if (params.contains(JS(transactions)))
    {
        if (!params.at(JS(transactions)).is_bool())
            return Status{RippledError::rpcINVALID_PARAMS, "transactionsFlagNotBool"};

        transactions = params.at(JS(transactions)).as_bool();
    }

    bool expand = false;
    if (params.contains(JS(expand)))
    {
        if (!params.at(JS(expand)).is_bool())
            return Status{RippledError::rpcINVALID_PARAMS, "expandFlagNotBool"};

        expand = params.at(JS(expand)).as_bool();
    }

    bool diff = false;
    if (params.contains("diff"))
    {
        if (!params.at("diff").is_bool())
            return Status{RippledError::rpcINVALID_PARAMS, "diffFlagNotBool"};

        diff = params.at("diff").as_bool();
    }

    if (params.contains(JS(full)))
        return Status{RippledError::rpcNOT_SUPPORTED};

    if (params.contains(JS(accounts)))
        return Status{RippledError::rpcNOT_SUPPORTED};

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    boost::json::object header;
    if (binary)
    {
        header[JS(ledger_data)] = ripple::strHex(ledgerInfoToBlob(lgrInfo));
    }
    else
    {
        header[JS(accepted)] = true;
        header[JS(account_hash)] = ripple::strHex(lgrInfo.accountHash);
        header[JS(close_flags)] = lgrInfo.closeFlags;
        header[JS(close_time)] = lgrInfo.closeTime.time_since_epoch().count();
        header[JS(close_time_human)] = ripple::to_string(lgrInfo.closeTime);
        header[JS(close_time_resolution)] = lgrInfo.closeTimeResolution.count();
        header[JS(closed)] = true;
        header[JS(hash)] = ripple::strHex(lgrInfo.hash);
        header[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
        header[JS(ledger_index)] = std::to_string(lgrInfo.seq);
        header[JS(parent_close_time)] = lgrInfo.parentCloseTime.time_since_epoch().count();
        header[JS(parent_hash)] = ripple::strHex(lgrInfo.parentHash);
        header[JS(seqNum)] = std::to_string(lgrInfo.seq);
        header[JS(totalCoins)] = ripple::to_string(lgrInfo.drops);
        header[JS(total_coins)] = ripple::to_string(lgrInfo.drops);
        header[JS(transaction_hash)] = ripple::strHex(lgrInfo.txHash);
    }
    header[JS(closed)] = true;

    if (transactions)
    {
        header[JS(transactions)] = boost::json::value(boost::json::array_kind);
        boost::json::array& jsonTxs = header.at(JS(transactions)).as_array();
        if (expand)
        {
            auto txns = context.backend->fetchAllTransactionsInLedger(lgrInfo.seq, context.yield);

            std::transform(
                std::move_iterator(txns.begin()),
                std::move_iterator(txns.end()),
                std::back_inserter(jsonTxs),
                [binary](auto obj) {
                    boost::json::object entry;
                    if (!binary)
                    {
                        auto [txn, meta] = toExpandedJson(obj);
                        entry = txn;
                        entry[JS(metaData)] = meta;
                    }
                    else
                    {
                        entry[JS(tx_blob)] = ripple::strHex(obj.transaction);
                        entry[JS(meta)] = ripple::strHex(obj.metadata);
                    }
                    // entry[JS(ledger_index)] = obj.ledgerSequence;
                    return entry;
                });
        }
        else
        {
            auto hashes = context.backend->fetchAllTransactionHashesInLedger(lgrInfo.seq, context.yield);
            std::transform(
                std::move_iterator(hashes.begin()),
                std::move_iterator(hashes.end()),
                std::back_inserter(jsonTxs),
                [](auto hash) {
                    boost::json::object entry;
                    return boost::json::string(ripple::strHex(hash));
                });
        }
    }

    if (diff)
    {
        header["diff"] = boost::json::value(boost::json::array_kind);
        boost::json::array& jsonDiff = header.at("diff").as_array();
        auto diff = context.backend->fetchLedgerDiff(lgrInfo.seq, context.yield);
        for (auto const& obj : diff)
        {
            boost::json::object entry;
            entry["object_id"] = ripple::strHex(obj.key);
            if (binary)
                entry["object"] = ripple::strHex(obj.blob);
            else if (obj.blob.size())
            {
                ripple::STLedgerEntry sle{ripple::SerialIter{obj.blob.data(), obj.blob.size()}, obj.key};
                entry["object"] = toJson(sle);
            }
            else
                entry["object"] = "";
            jsonDiff.push_back(std::move(entry));
        }
    }

    response[JS(ledger)] = header;
    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;
    return response;
}

}  // namespace RPC
