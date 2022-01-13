#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>
// Get state nodes from a ledger
//   Inputs:
//     limit:        integer, maximum number of entries
//     marker:       opaque, resume point
//     binary:       boolean, format
//     type:         string // optional, defaults to all ledger node types
//   Outputs:
//     ledger_hash:  chosen ledger's hash
//     ledger_index: chosen ledger's index
//     state:        array of state nodes
//     marker:       resume point, if any
//
//

namespace RPC {

using boost::json::value_to;

Result
doLedgerData(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    bool binary = false;
    if (request.contains("binary"))
    {
        if (!request.at("binary").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = request.at("binary").as_bool();
    }

    std::size_t limit = binary ? 2048 : 256;
    if (request.contains("limit"))
    {
        if (!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInteger"};

        limit = boost::json::value_to<int>(request.at("limit"));
    }

    std::optional<ripple::uint256> cursor;
    if (request.contains("marker"))
    {
        if (!request.at("marker").is_string())
            return Status{Error::rpcINVALID_PARAMS, "markerNotString"};

        BOOST_LOG_TRIVIAL(debug) << __func__ << " : parsing marker";

        cursor = ripple::uint256{};
        if (!cursor->parseHex(request.at("marker").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "markerMalformed"};
    }

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    Backend::LedgerPage page;
    auto start = std::chrono::system_clock::now();
    page = context.backend->fetchLedgerPage(
        cursor, lgrInfo.seq, limit, 0, context.yield);

    auto end = std::chrono::system_clock::now();

    auto time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    boost::json::object header;
    if (!cursor)
    {
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
            header["close_time_resolution"] =
                lgrInfo.closeTimeResolution.count();
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

            response["ledger"] = header;
        }
    }

    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

    boost::json::array objects;
    std::vector<Backend::LedgerObject>& results = page.objects;
    std::optional<ripple::uint256> const& returnedCursor = page.cursor;

    if (returnedCursor)
    {
        response["marker"] = ripple::strHex(*returnedCursor);
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " cursor = " << ripple::strHex(*returnedCursor);
    }

    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " number of results = " << results.size();
    for (auto const& [key, object] : results)
    {
        ripple::STLedgerEntry sle{
            ripple::SerialIter{object.data(), object.size()}, key};
        if (binary)
        {
            boost::json::object entry;
            entry["data"] = ripple::serializeHex(sle);
            entry["index"] = ripple::to_string(sle.key());
            objects.push_back(entry);
        }
        else
            objects.push_back(toJson(sle));
    }
    response["state"] = objects;

    if (cursor && page.warning)
    {
        response["warning"] =
            "Periodic database update in progress. Data for this ledger may be "
            "incomplete. Data should be complete "
            "within a few minutes. Other RPC calls are not affected, "
            "regardless of ledger. This "
            "warning is only present on the first "
            "page of the ledger";
    }

    return response;
}

}  // namespace RPC
