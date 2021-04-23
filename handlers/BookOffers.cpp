#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <handlers/RPCHelpers.h>
#include <reporting/BackendInterface.h>
#include <reporting/DBHelpers.h>
#include <reporting/Pg.h>

boost::json::object
doBookOffers(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    std::cout << "enter" << std::endl;
    boost::json::object response;

    auto ledgerSequence = ledgerSequenceFromRequest(request, backend);
    if (!ledgerSequence)
    {
        response["error"] = "Empty database";
        return response;
    }
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
                "Invalid field 'taker_pays.issuer', bad issuer account one.";
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
                "Invalid field 'taker_gets.issuer', bad issuer account one.";
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

    boost::optional<ripple::AccountID> takerID;
    if (request.contains("taker"))
    {
        if (!request.at("taker").is_string())
        {
            response["error"] = "taker should be string";
            return response;
        }

        takerID = ripple::parseBase58<ripple::AccountID>(
            request.at("taker").as_string().c_str());
        if (!takerID)
        {
            response["error"] = "Invalid taker";
            return response;
        }
    }

    if (pay_currency == get_currency && pay_issuer == get_issuer)
    {
        response["error"] = "Bad market";
        return response;
    }

    std::uint32_t limit = 200;
    if (request.contains("limit") and
        request.at("limit").kind() == boost::json::kind::int64)
        limit = request.at("limit").as_int64();

    std::optional<ripple::uint256> cursor;
    if (request.contains("cursor"))
    {
        cursor = ripple::uint256{};
        cursor->parseHex(request.at("cursor").as_string().c_str());
    }

    ripple::Book book = {
        {pay_currency, pay_issuer}, {get_currency, get_issuer}};

    ripple::uint256 bookBase = getBookBase(book);
    auto start = std::chrono::system_clock::now();
    auto [offers, retCursor] =
        backend.fetchBookOffers(bookBase, *ledgerSequence, limit, cursor);
    auto end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning) << "Time loading books from Postgres: "
                               << ((end - start).count() / 1000000000.0);

    response["offers"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonOffers = response.at("offers").as_array();

    start = std::chrono::system_clock::now();
    std::transform(
        std::move_iterator(offers.begin()),
        std::move_iterator(offers.end()),
        std::back_inserter(jsonOffers),
        [](auto obj) {
            try
            {
                ripple::SerialIter it{obj.blob.data(), obj.blob.size()};
                ripple::SLE offer{it, obj.key};
                ripple::uint256 bookDir = offer.getFieldH256(ripple::sfBookDirectory);

                boost::json::object offerJson = getJson(offer);
                offerJson["quality"] = ripple::amountFromQuality(getQuality(bookDir)).getText();
                return offerJson;
            }
            catch (std::exception const& e)
            {
                boost::json::object empty;
                empty["missing_key"] = ripple::strHex(obj.key);
                empty["data"] = ripple::strHex(obj.blob);
                return empty;
            }
        });

    end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning) << "Time transforming to json: "
                               << ((end - start).count() / 1000000000.0);
    if (retCursor)
        response["cursor"] = ripple::strHex(*retCursor);

    return response;
}
