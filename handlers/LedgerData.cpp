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

#include <ripple/app/ledger/LedgerToJson.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/json.hpp>
#include <handlers/RPCHelpers.h>
#include <reporting/ReportingBackend.h>
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
boost::json::object
doLedgerData(
    boost::json::object const& request,
    CassandraFlatMapBackend const& backend)
{
    boost::json::object response;
    uint32_t ledger = request.at("ledger_index").as_int64();

    std::optional<int64_t> marker = request.contains("marker")
        ? request.at("marker").as_int64()
        : std::optional<int64_t>{};
    bool binary =
        request.contains("binary") ? request.at("binary").as_bool() : false;
    size_t limit = request.contains("limit") ? request.at("limit").as_int64()
                                             : (binary ? 2048 : 256);
    std::pair<
        std::vector<CassandraFlatMapBackend::LedgerObject>,
        std::optional<int64_t>>
        resultsPair;
    auto start = std::chrono::system_clock::now();
    if (request.contains("version"))
    {
        resultsPair = backend.doUpperBound2(marker, ledger, limit);
    }
    else
    {
        resultsPair = backend.doUpperBound(marker, ledger, limit);
    }

    auto end = std::chrono::system_clock::now();

    auto time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    boost::json::array objects;
    std::vector<CassandraFlatMapBackend::LedgerObject>& results =
        resultsPair.first;
    std::optional<int64_t>& returnedMarker = resultsPair.second;
    BOOST_LOG_TRIVIAL(debug)
        << "doUpperBound returned " << results.size() << " results";
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
            objects.push_back(getJson(sle));
    }
    response["objects"] = objects;
    if (returnedMarker)
        response["marker"] = returnedMarker.value();

    response["num_results"] = results.size();
    response["db_time"] = time;
    response["time_per_result"] = time / results.size();
    return response;
}

/*
std::pair<org::xrpl::rpc::v1::GetLedgerDataResponse, grpc::Status>
doLedgerDataGrpc(
    RPC::GRPCContext<org::xrpl::rpc::v1::GetLedgerDataRequest>& context)
{
    org::xrpl::rpc::v1::GetLedgerDataRequest& request = context.params;
    org::xrpl::rpc::v1::GetLedgerDataResponse response;
    grpc::Status status = grpc::Status::OK;

    std::shared_ptr<ReadView const> ledger;
    if (RPC::ledgerFromRequest(ledger, context))
    {
        grpc::Status errorStatus{
            grpc::StatusCode::NOT_FOUND, "ledger not found"};
        return {response, errorStatus};
    }

    ReadView::key_type key = ReadView::key_type();
    if (request.marker().size() != 0)
    {
        key = uint256::fromVoid(request.marker().data());
        if (key.size() != request.marker().size())
        {
            grpc::Status errorStatus{
                grpc::StatusCode::INVALID_ARGUMENT, "marker malformed"};
            return {response, errorStatus};
        }
    }

    auto e = ledger->sles.end();
    ReadView::key_type stopKey = ReadView::key_type();
    if (request.end_marker().size() != 0)
    {
        stopKey = uint256::fromVoid(request.end_marker().data());
        if (stopKey.size() != request.marker().size())
        {
            grpc::Status errorStatus{
                grpc::StatusCode::INVALID_ARGUMENT, "end marker malformed"};
            return {response, errorStatus};
        }
        e = ledger->sles.upper_bound(stopKey);
    }

    int maxLimit = RPC::Tuning::pageLength(true);

    for (auto i = ledger->sles.upper_bound(key); i != e; ++i)
    {
        auto sle = ledger->read(keylet::unchecked((*i)->key()));
        if (maxLimit-- <= 0)
        {
            // Stop processing before the current key.
            auto k = sle->key();
            --k;
            response.set_marker(k.data(), k.size());
            break;
        }
        auto stateObject = response.mutable_ledger_objects()->add_objects();
        Serializer s;
        sle->add(s);
        stateObject->set_data(s.peekData().data(), s.getLength());
        stateObject->set_key(sle->key().data(), sle->key().size());
    }
    return {response, status};
}
*/
