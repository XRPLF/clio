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
#include <backend/Pg.h>

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

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if (!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }

    ripple::AccountID takerID = beast::zero;
    if (request.contains("taker"))
    {
        auto parsed = parseTaker(request["taker"]);
        if (auto status = std::get_if<Status>(&parsed))
            return *status;
        else
        {
            takerID = std::get<ripple::AccountID>(parsed);
        }
    }

    ripple::uint256 cursor = beast::zero;
    if (request.contains("cursor"))
    {
        if (!request.at("cursor").is_string())
            return Status{Error::rpcINVALID_PARAMS, "cursorNotString"};

        if (!cursor.parseHex(request.at("cursor").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedCursor"};
    }

    auto start = std::chrono::system_clock::now();
    auto [offers, retCursor, warning] =
        context.backend->fetchBookOffers(bookBase, lgrInfo.seq, limit, cursor);
    auto end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning)
        << "Time loading books: " << ((end - start).count() / 1000000000.0);

    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

    response["offers"] = postProcessOrderBook(
        offers, book, takerID, *context.backend, lgrInfo.seq);

    end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning) << "Time transforming to json: "
                               << ((end - start).count() / 1000000000.0);

    if (retCursor)
        response["marker"] = ripple::strHex(*retCursor);
    if (warning)
        response["warning"] =
            "Periodic database update in progress. Data for this book as of "
            "this ledger "
            "may be incomplete. Data should be complete within one minute";

    return response;
}

}  // namespace RPC
