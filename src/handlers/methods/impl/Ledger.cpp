#include <handlers/methods/Ledger.h>
#include <handlers/RPCHelpers.h>
#include <backend/BackendInterface.h>

namespace RPC
{

Status
Ledger::check()
{
    auto params = context_.params;

    bool binary = false;
    if(params.contains("binary"))
    {
        if(!params.at("binary").is_bool())
            return {Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};
        
        binary = params.at("binary").as_bool();
    }

    bool transactions = false;
    if(params.contains("transactions"))
    {
        if(!params.at("transactions").is_bool())
            return {Error::rpcINVALID_PARAMS, "transactionsFlagNotBool"};
        
        transactions = params.at("transactions").as_bool();
    }

    bool expand = false;
    if(params.contains("expand"))
    {
        if(!params.at("expand").is_bool())
            return {Error::rpcINVALID_PARAMS, "expandFlagNotBool"};
        
        expand = params.at("expand").as_bool();
    }

    auto v = ledgerInfoFromRequest(context_);
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
        header["close_time_human"] = ripple::to_string(lgrInfo.closeTime);;
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

    if (transactions)
    {
        header["transactions"] = boost::json::value(boost::json::array_kind);
        boost::json::array& jsonTxs = header.at("transactions").as_array();
        if (expand)
        {
            auto txns = context_.backend->fetchAllTransactionsInLedger(lgrInfo.seq);

            std::transform(
                std::move_iterator(txns.begin()),
                std::move_iterator(txns.end()),
                std::back_inserter(jsonTxs),
                [binary](auto obj) {
                    boost::json::object entry;
                    if (!binary)
                    {
                        auto [sttx, meta] = deserializeTxPlusMeta(obj);
                        entry = toJson(*sttx);
                        entry["metaData"] = toJson(*meta);
                    }
                    else
                    {
                        entry["tx_blob"] = ripple::strHex(obj.transaction);
                        entry["meta"] = ripple::strHex(obj.metadata);
                    }
                    entry["ledger_sequence"] = obj.ledgerSequence;
                    return entry;
                });
        }
        else
        {
            auto hashes =
                context_.backend->fetchAllTransactionHashesInLedger(lgrInfo.seq);
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
    
    response_["ledger"] = header;
    response_["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response_["ledger_index"] = lgrInfo.seq;
    return OK;
}

}