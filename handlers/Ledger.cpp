#include <handlers/RPCHelpers.h>
#include <reporting/BackendInterface.h>

boost::json::object
doLedger(boost::json::object const& request, BackendInterface const& backend)
{
    boost::json::object response;
    if (!request.contains("ledger_index"))
    {
        response["error"] = "Please specify a ledger index";
        return response;
    }
    auto ledgerSequence = ledgerSequenceFromRequest(request, backend);
    if (!ledgerSequence)
    {
        response["error"] = "Empty database";
        return response;
    }

    auto lgrInfo = backend.fetchLedgerBySequence(*ledgerSequence);
    if (!lgrInfo)
    {
        response["error"] = "ledger not found";
        return response;
    }
    boost::json::object header;
    header["ledger_sequence"] = lgrInfo->seq;
    header["ledger_hash"] = ripple::strHex(lgrInfo->hash);
    header["txns_hash"] = ripple::strHex(lgrInfo->txHash);
    header["state_hash"] = ripple::strHex(lgrInfo->accountHash);
    header["parent_hash"] = ripple::strHex(lgrInfo->parentHash);
    header["total_coins"] = ripple::to_string(lgrInfo->drops);
    header["close_flags"] = lgrInfo->closeFlags;

    // Always show fields that contribute to the ledger hash
    header["parent_close_time"] =
        lgrInfo->parentCloseTime.time_since_epoch().count();
    header["close_time"] = lgrInfo->closeTime.time_since_epoch().count();
    header["close_time_resolution"] = lgrInfo->closeTimeResolution.count();
    auto txns = backend.fetchAllTransactionsInLedger(*ledgerSequence);
    response["transactions"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonTransactions =
        response.at("transactions").as_array();

    std::transform(
        std::move_iterator(txns.begin()),
        std::move_iterator(txns.end()),
        std::back_inserter(jsonTransactions),
        [](auto obj) {
            boost::json::object entry;
            auto [sttx, meta] = deserializeTxPlusMeta(obj);
            entry["transaction"] = getJson(*sttx);
            entry["meta"] = getJson(*meta);
            return entry;
        });

    return response;
}
