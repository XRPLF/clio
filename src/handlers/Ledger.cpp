#include <handlers/RPCHelpers.h>
#include <backend/BackendInterface.h>

boost::json::object
doLedger(boost::json::object const& request, BackendInterface const& backend)
{
    boost::json::object response;
    if (!request.contains("ledger_index"))
    {
        response["error"] = "Please specify a ledger index";
        return response;
    }
    bool binary =
        request.contains("binary") ? request.at("binary").as_bool() : false;
    bool getTransactions = request.contains("transactions")
        ? request.at("transactions").as_bool()
        : false;
    bool expand =
        request.contains("expand") ? request.at("expand").as_bool() : false;
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
    if (binary)
    {
        header["blob"] = ripple::strHex(ledgerInfoToBlob(*lgrInfo));
    }
    else
    {
        header = toJson(*lgrInfo);
    }
    response["header"] = header;
    if (getTransactions)
    {
        response["transactions"] = boost::json::value(boost::json::array_kind);
        boost::json::array& jsonTransactions =
            response.at("transactions").as_array();
        if (expand)
        {
            auto txns = backend.fetchAllTransactionsInLedger(*ledgerSequence);

            std::transform(
                std::move_iterator(txns.begin()),
                std::move_iterator(txns.end()),
                std::back_inserter(jsonTransactions),
                [binary](auto obj) {
                    boost::json::object entry;
                    if (!binary)
                    {
                        auto [sttx, meta] = deserializeTxPlusMeta(obj);
                        entry["transaction"] = toJson(*sttx);
                        entry["metadata"] = toJson(*meta);
                    }
                    else
                    {
                        entry["transaction"] = ripple::strHex(obj.transaction);
                        entry["metadata"] = ripple::strHex(obj.metadata);
                    }
                    entry["ledger_sequence"] = obj.ledgerSequence;
                    return entry;
                });
        }
        else
        {
            auto hashes =
                backend.fetchAllTransactionHashesInLedger(*ledgerSequence);
            std::transform(
                std::move_iterator(hashes.begin()),
                std::move_iterator(hashes.end()),
                std::back_inserter(jsonTransactions),
                [](auto hash) {
                    boost::json::object entry;
                    entry["hash"] = ripple::strHex(hash);
                    return entry;
                });
        }
    }
    return response;
}
