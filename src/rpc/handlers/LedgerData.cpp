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
    bool outOfOrder = false;
    if (request.contains("out_of_order"))
    {
        if (!request.at("out_of_order").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};
        outOfOrder = request.at("out_of_order").as_bool();
    }

    std::optional<ripple::uint256> cursor;
    std::optional<uint32_t> diffCursor;
    if (request.contains("marker"))
    {
        if (!request.at("marker").is_string())
        {
            if (outOfOrder)
            {
                if (!request.at("marker").is_int64())
                    return Status{
                        Error::rpcINVALID_PARAMS, "markerNotStringOrInt"};
                diffCursor = value_to<uint32_t>(request.at("marker"));
            }
            else
                return Status{Error::rpcINVALID_PARAMS, "markerNotString"};
        }
        else
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " : parsing marker";

            cursor = ripple::uint256{};
            if (!cursor->parseHex(request.at("marker").as_string().c_str()))
                return Status{Error::rpcINVALID_PARAMS, "markerMalformed"};
        }
    }

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);
    boost::json::object header;
    // no cursor means this is the first call, so we return header info
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

    auto start = std::chrono::system_clock::now();
    std::vector<Backend::LedgerObject> results;
    if (diffCursor)
    {
        assert(outOfOrder);
        auto diff =
            context.backend->fetchLedgerDiff(*diffCursor, context.yield);
        std::vector<ripple::uint256> keys;
        for (auto&& [key, object] : diff)
        {
            if (!object.size())
            {
                keys.push_back(std::move(key));
            }
        }
        auto objs = context.backend->fetchLedgerObjects(
            keys, lgrInfo.seq, context.yield);
        for (size_t i = 0; i < objs.size(); ++i)
        {
            auto&& obj = objs[i];
            if (obj.size())
                results.push_back({std::move(keys[i]), std::move(obj)});
        }
        if (*diffCursor > lgrInfo.seq)
            response["marker"] = *diffCursor - 1;
    }
    else
    {
        auto page = context.backend->fetchLedgerPage(
            cursor, lgrInfo.seq, limit, outOfOrder, context.yield);
        results = std::move(page.objects);
        if (page.cursor)
            response["marker"] = ripple::strHex(*(page.cursor));
        else if (outOfOrder)
            response["marker"] =
                context.backend->fetchLedgerRange()->maxSequence;
    }
    auto end = std::chrono::system_clock::now();

    auto time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " number of results = " << results.size()
        << " fetched in " << time << " microseconds";
    boost::json::array objects;
    objects.reserve(results.size());
    for (auto const& [key, object] : results)
    {
        ripple::STLedgerEntry sle{
            ripple::SerialIter{object.data(), object.size()}, key};
        if (binary)
        {
            boost::json::object entry;
            entry["data"] = ripple::serializeHex(sle);
            entry["index"] = ripple::to_string(sle.key());
            objects.push_back(std::move(entry));
        }
        else
            objects.push_back(toJson(sle));
    }
    response["state"] = std::move(objects);
    auto end2 = std::chrono::system_clock::now();

    time = std::chrono::duration_cast<std::chrono::microseconds>(end2 - end)
               .count();
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " number of results = " << results.size()
        << " serialized in " << time << " microseconds";

    return response;
}

}  // namespace RPC
