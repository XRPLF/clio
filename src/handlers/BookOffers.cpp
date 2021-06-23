#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <handlers/RPCHelpers.h>

boost::json::object
doBookOffers(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    boost::json::object response;

    auto ledgerSequence = ledgerSequenceFromRequest(request, backend);
    if (!ledgerSequence)
    {
        response["error"] = "Empty database";
        return response;
    }
    ripple::uint256 bookBase;
    if (request.contains("book"))
    {
        if (!bookBase.parseHex(request.at("book").as_string().c_str()))
        {
            response["error"] = "Error parsing book";
            return response;
        }
    }
    else
    {
        if (!request.contains("taker_pays"))
        {
            response["error"] = "Missing field taker_pays";
            return response;
        }

        if (!request.contains("taker_gets"))
        {
            response["error"] = "Missing field taker_gets";
            return response;
        }

        boost::json::object taker_pays;
        if (request.at("taker_pays").kind() == boost::json::kind::object)
        {
            taker_pays = request.at("taker_pays").as_object();
        }
        else
        {
            response["error"] = "Invalid field taker_pays";
            return response;
        }

        boost::json::object taker_gets;
        if (request.at("taker_gets").kind() == boost::json::kind::object)
        {
            taker_gets = request.at("taker_gets").as_object();
        }
        else
        {
            response["error"] = "Invalid field taker_gets";
            return response;
        }

        if (!taker_pays.contains("currency"))
        {
            response["error"] = "Missing field taker_pays.currency";
            return response;
        }

        if (!taker_pays.at("currency").is_string())
        {
            response["error"] = "taker_pays.currency should be string";
            return response;
        }

        if (!taker_gets.contains("currency"))
        {
            response["error"] = "Missing field taker_gets.currency";
            return response;
        }

        if (!taker_gets.at("currency").is_string())
        {
            response["error"] = "taker_gets.currency should be string";
            return response;
        }

        ripple::Currency pay_currency;

        if (!ripple::to_currency(
                pay_currency, taker_pays.at("currency").as_string().c_str()))
        {
            response["error"] =
                "Invalid field 'taker_pays.currency', bad currency.";
            return response;
        }

        ripple::Currency get_currency;

        if (!ripple::to_currency(
                get_currency, taker_gets["currency"].as_string().c_str()))
        {
            response["error"] =
                "Invalid field 'taker_gets.currency', bad currency.";
            return response;
        }

        ripple::AccountID pay_issuer;

        if (taker_pays.contains("issuer"))
        {
            if (!taker_pays.at("issuer").is_string())
            {
                response["error"] = "taker_pays.issuer should be string";
                return response;
            }

            if (!ripple::to_issuer(
                    pay_issuer, taker_pays.at("issuer").as_string().c_str()))
            {
                response["error"] =
                    "Invalid field 'taker_pays.issuer', bad issuer.";
                return response;
            }

            if (pay_issuer == ripple::noAccount())
            {
                response["error"] =
                    "Invalid field 'taker_pays.issuer', bad issuer account "
                    "one.";
                return response;
            }
        }
        else
        {
            pay_issuer = ripple::xrpAccount();
        }

        if (isXRP(pay_currency) && !isXRP(pay_issuer))
        {
            response["error"] =
                "Unneeded field 'taker_pays.issuer' for XRP currency "
                "specification.";
            return response;
        }

        if (!isXRP(pay_currency) && isXRP(pay_issuer))
        {
            response["error"] =
                "Invalid field 'taker_pays.issuer', expected non-XRP issuer.";
            return response;
        }

        ripple::AccountID get_issuer;

        if (taker_gets.contains("issuer"))
        {
            if (!taker_gets["issuer"].is_string())
            {
                response["error"] = "taker_gets.issuer should be string";
                return response;
            }

            if (!ripple::to_issuer(
                    get_issuer, taker_gets.at("issuer").as_string().c_str()))
            {
                response["error"] =
                    "Invalid field 'taker_gets.issuer', bad issuer.";
                return response;
            }

            if (get_issuer == ripple::noAccount())
            {
                response["error"] =
                    "Invalid field 'taker_gets.issuer', bad issuer account "
                    "one.";
                return response;
            }
        }
        else
        {
            get_issuer = ripple::xrpAccount();
        }

        if (ripple::isXRP(get_currency) && !ripple::isXRP(get_issuer))
        {
            response["error"] =
                "Unneeded field 'taker_gets.issuer' for XRP currency "
                "specification.";
            return response;
        }

        if (!ripple::isXRP(get_currency) && ripple::isXRP(get_issuer))
        {
            response["error"] =
                "Invalid field 'taker_gets.issuer', expected non-XRP issuer.";
            return response;
        }

        if (pay_currency == get_currency && pay_issuer == get_issuer)
        {
            response["error"] = "Bad market";
            return response;
        }
        ripple::Book book = {
            {pay_currency, pay_issuer}, {get_currency, get_issuer}};

        bookBase = getBookBase(book);
    }

    std::uint32_t limit = 200;
    if (request.contains("limit") and
        request.at("limit").kind() == boost::json::kind::int64)
        limit = request.at("limit").as_int64();

    std::optional<ripple::AccountID> takerID = {};
    if (request.contains("taker"))
    {
        if (!request.at("taker").is_string())
        {
            response["error"] = "Taker account must be string";
            return response;
        }

        takerID =
            accountFromStringStrict(request.at("taker").as_string().c_str());
        if (!takerID)
        {
            response["error"] = "Invalid taker account";
            return response;
        }
    }

    std::optional<ripple::uint256> cursor;
    if (request.contains("cursor"))
    {
        cursor = ripple::uint256{};
        if (!cursor->parseHex(request.at("cursor").as_string().c_str()))
        {
            response["error"] = "Bad cursor";
            return response;
        }
    }

    auto start = std::chrono::system_clock::now();
    auto [offers, retCursor, warning] =
        backend.fetchBookOffers(bookBase, *ledgerSequence, limit, cursor);
    auto end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning)
        << "Time loading books: " << ((end - start).count() / 1000000000.0);

    if (warning)
        response["warning"] = *warning;

    response["offers"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonOffers = response.at("offers").as_array();

    start = std::chrono::system_clock::now();
    for (auto const& obj : offers)
    {
        if (jsonOffers.size() == limit)
            break;

        try
        {
            ripple::SerialIter it{obj.blob.data(), obj.blob.size()};
            ripple::SLE offer{it, obj.key};
            ripple::uint256 bookDir =
                offer.getFieldH256(ripple::sfBookDirectory);

            boost::json::object offerJson = toJson(offer);
            offerJson["quality"] =
                ripple::amountFromQuality(getQuality(bookDir)).getText();
            jsonOffers.push_back(offerJson);
        }
        catch (std::exception const& e)
        {
        }
    }

    end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning) << "Time transforming to json: "
                               << ((end - start).count() / 1000000000.0);
    if (retCursor)
        response["cursor"] = ripple::strHex(*retCursor);
    if (warning)
        response["warning"] =
            "Periodic database update in progress. Data for this book as of "
            "this ledger "
            "may be incomplete. Data should be complete within one minute";

    return response;
}
