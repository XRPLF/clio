//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

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

#include <boost/json.hpp>
#include <reporting/ReportingETL.h>

namespace ripple {

boost::json::object
forwardToP2p(boost::json::object const& request, ReportingETL& etl)
{
    return etl.getETLLoadBalancer().forwardToP2p(request);
}

std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>
getP2pForwardingStub(ReportingETL& etl)
{
    return etl
        .getETLLoadBalancer()
        .getP2pForwardingStub();
}

// We only forward requests where ledger_index is "current" or "closed"
// otherwise, attempt to handle here
bool
shouldForwardToP2p(boost::json::object const& request)
{
    std::string strCommand = request.contains("command")
        ? request.at("command").as_string().c_str()
        : request.at("method").as_string().c_str();

    BOOST_LOG_TRIVIAL(info) << "COMMAND:" << strCommand;
    BOOST_LOG_TRIVIAL(info) << "REQUEST:" << request;

    auto handler = forwardCommands.find(strCommand) != forwardCommands.end();
    if (!handler)
    {
        BOOST_LOG_TRIVIAL(error) 
            << "Error getting handler. command = " << strCommand;
        return false;
    }

    if (request.contains("ledger_index"))
    {
        auto indexValue = request.at("ledger_index");
        if (!indexValue.is_uint64())
        {
            std::string index = indexValue.as_string().c_str();
            return index == "current" || index == "closed";
        }
    }

    return true;
}

}  // namespace ripple
