#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <rpc/RPCHelpers.h>

#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>

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
            return Status{Error::rpcINVALID_PARAMS, "bookNotString"};

        if (!bookBase.parseHex(request.at("book").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "invalidBook"};
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

    ripple::uint256 marker = beast::zero;
    if (auto const status = getHexMarker(request, marker); status)
        return status;

    auto start = std::chrono::system_clock::now();
    auto [offers, retMarker] = context.backend->fetchBookOffers(
        bookBase, lgrInfo.seq, limit, marker, context.yield);
    auto end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning)
        << "Time loading books: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
               .count()
        << " milliseconds - request = " << request;

    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;

    response[JS(offers)] = postProcessOrderBook(
        offers, book, takerID, *context.backend, lgrInfo.seq, context.yield);

    auto end2 = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning)
        << "Time transforming to json: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end2 - end)
               .count()
        << " milliseconds - request = " << request;

    if (retMarker)
        response["marker"] = ripple::strHex(*retMarker);

    return response;
}

}  // namespace RPC
