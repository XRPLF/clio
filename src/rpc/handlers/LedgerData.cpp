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

    bool const binary = getBool(request, "binary", false);

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    if (!binary)
        limit = std::clamp(limit, {1}, {256});

    bool outOfOrder = false;
    if (request.contains("out_of_order"))
    {
        if (!request.at("out_of_order").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};
        outOfOrder = request.at("out_of_order").as_bool();
    }

    std::optional<ripple::uint256> marker;
    std::optional<uint32_t> diffMarker;
    if (request.contains(JS(marker)))
    {
        if (!request.at(JS(marker)).is_string())
        {
            if (outOfOrder)
            {
                if (!request.at(JS(marker)).is_int64())
                    return Status{
                        Error::rpcINVALID_PARAMS, "markerNotStringOrInt"};
                diffMarker = value_to<uint32_t>(request.at(JS(marker)));
            }
            else
                return Status{Error::rpcINVALID_PARAMS, "markerNotString"};
        }
        else
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " : parsing marker";

            marker = ripple::uint256{};
            if (!marker->parseHex(request.at(JS(marker)).as_string().c_str()))
                return Status{Error::rpcINVALID_PARAMS, "markerMalformed"};
        }
    }

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    boost::json::object header;
    // no marker means this is the first call, so we return header info
    if (!request.contains(JS(marker)))
    {
        if (binary)
        {
            header[JS(ledger_data)] = ripple::strHex(ledgerInfoToBlob(lgrInfo));
        }
        else
        {
            header[JS(accepted)] = true;
            header[JS(account_hash)] = ripple::strHex(lgrInfo.accountHash);
            header[JS(close_flags)] = lgrInfo.closeFlags;
            header[JS(close_time)] =
                lgrInfo.closeTime.time_since_epoch().count();
            header[JS(close_time_human)] = ripple::to_string(lgrInfo.closeTime);
            header[JS(close_time_resolution)] =
                lgrInfo.closeTimeResolution.count();
            header[JS(closed)] = true;
            header[JS(hash)] = ripple::strHex(lgrInfo.hash);
            header[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
            header[JS(ledger_index)] = std::to_string(lgrInfo.seq);
            header[JS(parent_close_time)] =
                lgrInfo.parentCloseTime.time_since_epoch().count();
            header[JS(parent_hash)] = ripple::strHex(lgrInfo.parentHash);
            header[JS(seqNum)] = std::to_string(lgrInfo.seq);
            header[JS(totalCoins)] = ripple::to_string(lgrInfo.drops);
            header[JS(total_coins)] = ripple::to_string(lgrInfo.drops);
            header[JS(transaction_hash)] = ripple::strHex(lgrInfo.txHash);
        }

        response[JS(ledger)] = header;
    }
    else
    {
        if (!outOfOrder &&
            !context.backend->fetchLedgerObject(
                *marker, lgrInfo.seq, context.yield))
            return Status{Error::rpcINVALID_PARAMS, "markerDoesNotExist"};
    }

    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;

    auto start = std::chrono::system_clock::now();
    std::vector<Backend::LedgerObject> results;
    if (diffMarker)
    {
        assert(outOfOrder);
        auto diff =
            context.backend->fetchLedgerDiff(*diffMarker, context.yield);
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
        if (*diffMarker > lgrInfo.seq)
            response["marker"] = *diffMarker - 1;
    }
    else
    {
        auto page = context.backend->fetchLedgerPage(
            marker, lgrInfo.seq, limit, outOfOrder, context.yield);
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
            entry[JS(data)] = ripple::serializeHex(sle);
            entry[JS(index)] = ripple::to_string(sle.key());
            objects.push_back(std::move(entry));
        }
        else
            objects.push_back(toJson(sle));
    }
    response[JS(state)] = std::move(objects);
    if (outOfOrder)
        response["cache_full"] = context.backend->cache().isFull();
    auto end2 = std::chrono::system_clock::now();

    time = std::chrono::duration_cast<std::chrono::microseconds>(end2 - end)
               .count();
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " number of results = " << results.size()
        << " serialized in " << time << " microseconds";

    return response;
}

}  // namespace RPC
