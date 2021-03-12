#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/json.hpp>
#include <handlers/RPCHelpers.h>
#include <reporting/BackendInterface.h>
// {
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   ...
// }
boost::json::object
doLedgerEntry(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    boost::json::object response;
    bool binary =
        request.contains("binary") ? request.at("binary").as_bool() : false;
    auto ledgerSequence = ledgerSequenceFromRequest(request, backend);
    if (!ledgerSequence)
    {
        response["error"] = "Empty database";
        return response;
    }
    ripple::uint256 key;
    if (!key.parseHex(request.at("index").as_string().c_str()))
    {
        response["error"] = "Error parsing index";
        return response;
    }
    auto start = std::chrono::system_clock::now();
    auto dbResponse = backend.fetchLedgerObject(key, *ledgerSequence);
    auto end = std::chrono::system_clock::now();
    auto time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    if (!dbResponse or dbResponse->size() == 0)
    {
        response["error"] = "Object not found";
        return response;
    }
    if (binary)
    {
        response["object"] = ripple::strHex(*dbResponse);
    }
    else
    {
        ripple::STLedgerEntry sle{
            ripple::SerialIter{dbResponse->data(), dbResponse->size()}, key};
        response["object"] = getJson(sle);
    }

    return response;
}

