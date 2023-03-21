//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <log/Logger.h>
#include <rpc/RPCHelpers.h>

#include <boost/json.hpp>

#include <algorithm>

using namespace clio;

// local to compilation unit loggers
namespace {
clio::Logger gLog{"RPC"};
}  // namespace

namespace RPC {

Result
doBookOffers(Context const& context)
{
    auto request = context.params;

    boost::json::object response = {};
    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    ripple::Book book;
    ripple::uint256 bookBase;
    if (request.contains("book"))
    {
        if (!request.at("book").is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "bookNotString"};

        if (!bookBase.parseHex(request.at("book").as_string().c_str()))
            return Status{RippledError::rpcINVALID_PARAMS, "invalidBook"};
    }
    else
    {
        auto parsed = parseBook(request);
        if (auto status = std::get_if<Status>(&parsed))
            return *status;
        else
        {
            book = std::get<ripple::Book>(parsed);
            bookBase = getBookBase(book);
        }
    }

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    ripple::AccountID takerID = beast::zero;
    if (auto const status = getTaker(request, takerID); status)
        return status;

    auto start = std::chrono::system_clock::now();
    auto [offers, _] = context.backend->fetchBookOffers(
        bookBase, lgrInfo.seq, limit, context.yield);
    auto end = std::chrono::system_clock::now();

    gLog.warn() << "Time loading books: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       end - start)
                       .count()
                << " milliseconds - request = " << request;

    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;

    response[JS(offers)] = postProcessOrderBook(
        offers, book, takerID, *context.backend, lgrInfo.seq, context.yield);

    auto end2 = std::chrono::system_clock::now();

    gLog.warn() << "Time transforming to json: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       end2 - end)
                       .count()
                << " milliseconds - request = " << request;
    return response;
}

}  // namespace RPC
