#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doLedger(Context const& context)
{
    auto params = context.params;
    boost::json::object response = {};

    bool binary = false;
    if (params.contains("binary"))
    {
        if (!params.at("binary").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = params.at("binary").as_bool();
    }

    bool transactions = false;
    if (params.contains("transactions"))
    {
        if (!params.at("transactions").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "transactionsFlagNotBool"};

        transactions = params.at("transactions").as_bool();
    }

    bool expand = false;
    if (params.contains("expand"))
    {
        if (!params.at("expand").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "expandFlagNotBool"};

        expand = params.at("expand").as_bool();
    }

    bool diff = false;
    if (params.contains("diff"))
    {
        if (!params.at("diff").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "diffFlagNotBool"};

        diff = params.at("diff").as_bool();
    }

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    boost::json::object header;
    if (binary)
    {
        header["ledger_data"] = ripple::strHex(ledgerInfoToBlob(lgrInfo));
    }
    else
    {
        header["accepted"] = true;
        header["account_hash"] = ripple::strHex(lgrInfo.accountHash);
        header["close_flags"] = lgrInfo.closeFlags;
        header["close_time"] = lgrInfo.closeTime.time_since_epoch().count();
        header["close_time_human"] = ripple::to_string(lgrInfo.closeTime);
        ;
        header["close_time_resolution"] = lgrInfo.closeTimeResolution.count();
        header["closed"] = true;
        header["hash"] = ripple::strHex(lgrInfo.hash);
        header["ledger_hash"] = ripple::strHex(lgrInfo.hash);
        header["ledger_index"] = std::to_string(lgrInfo.seq);
        header["parent_close_time"] =
            lgrInfo.parentCloseTime.time_since_epoch().count();
        header["parent_hash"] = ripple::strHex(lgrInfo.parentHash);
        header["seqNum"] = std::to_string(lgrInfo.seq);
        header["totalCoins"] = ripple::to_string(lgrInfo.drops);
        header["total_coins"] = ripple::to_string(lgrInfo.drops);
        header["transaction_hash"] = ripple::strHex(lgrInfo.txHash);
    }
    header["closed"] = true;

    if (transactions)
    {
        header["transactions"] = boost::json::value(boost::json::array_kind);
        boost::json::array& jsonTxs = header.at("transactions").as_array();
        if (expand)
        {
            auto txns = context.backend->fetchAllTransactionsInLedger(
                lgrInfo.seq, context.yield);

            std::transform(
                std::move_iterator(txns.begin()),
                std::move_iterator(txns.end()),
                std::back_inserter(jsonTxs),
                [binary, &context](auto obj) {
                    boost::json::object entry;
                    if (!binary)
                    {
                        auto [txn, meta] =
                            toExpandedJson(obj, *context.backend);
                        entry = txn;
                        entry["metaData"] = meta;
                    }
                    else
                    {
                        entry["tx_blob"] = ripple::strHex(obj.transaction);
                        entry["meta"] = ripple::strHex(obj.metadata);
                    }
                    // entry["ledger_index"] = obj.ledgerSequence;
                    return entry;
                });
        }
        else
        {
            auto hashes = context.backend->fetchAllTransactionHashesInLedger(
                lgrInfo.seq, context.yield);
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
        auto diff =
            context.backend->fetchLedgerDiff(lgrInfo.seq, context.yield);
        for (auto const& obj : diff)
        {
            boost::json::object entry;
            entry["id"] = ripple::strHex(obj.key);
            if (binary)
                entry["object"] = ripple::strHex(obj.blob);
            else if (obj.blob.size())
            {
                ripple::STLedgerEntry sle{
                    ripple::SerialIter{obj.blob.data(), obj.blob.size()},
                    obj.key};
                entry["object"] = toJson(sle, *context.backend, lgrInfo.seq);
            }
            else
                entry["object"] = "";
            jsonDiff.push_back(std::move(entry));
        }
    }

    response["ledger"] = header;
    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;
    return response;
}

}  // namespace RPC
