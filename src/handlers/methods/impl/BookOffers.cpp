#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <handlers/RPCHelpers.h>
#include <handlers/methods/Exchange.h>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <backend/Pg.h>

namespace RPC
{

Status
BookOffers::check()
{
    auto request = context_.params;

    auto v = ledgerInfoFromRequest(context_);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    ripple::uint256 bookBase;
    if (request.contains("book"))
    {
        if (!request.at("book").is_string())
            return {Error::rpcINVALID_PARAMS, "bookNotString"};

        if (!bookBase.parseHex(request.at("book").as_string().c_str()))
            return {Error::rpcINVALID_PARAMS, "invalidBook"};
    }
    else
    {
        if (!request.contains("taker_pays"))
            return {Error::rpcINVALID_PARAMS, "missingTakerPays"};

        if (!request.contains("taker_gets"))
            return {Error::rpcINVALID_PARAMS, "missingTakerGets"};

        if (!request.at("taker_pays").is_object())
            return {Error::rpcINVALID_PARAMS, "takerPaysNotObject"};
        
        if (!request.at("taker_gets").is_object())
            return {Error::rpcINVALID_PARAMS, "takerGetsNotObject"};

        auto taker_pays = request.at("taker_pays").as_object();
        if (!taker_pays.contains("currency"))
            return {Error::rpcINVALID_PARAMS, "missingTakerPaysCurrency"};

        if (!taker_pays.at("currency").is_string())
            return {Error::rpcINVALID_PARAMS, "takerPaysCurrencyNotString"};

        auto taker_gets = request.at("taker_gets").as_object();
        if (!taker_gets.contains("currency"))
            return {Error::rpcINVALID_PARAMS, "missingTakerGetsCurrency"};

        if (!taker_gets.at("currency").is_string())
            return {Error::rpcINVALID_PARAMS, "takerGetsCurrencyNotString"};

        ripple::Currency pay_currency;
        if (!ripple::to_currency(
                pay_currency, taker_pays.at("currency").as_string().c_str()))
            return {Error::rpcINVALID_PARAMS, "badTakerPaysCurrency"};

        ripple::Currency get_currency;
        if (!ripple::to_currency(
                get_currency, taker_gets["currency"].as_string().c_str()))
            return {Error::rpcINVALID_PARAMS, "badTakerGetsCurrency"};

        ripple::AccountID pay_issuer;
        if (taker_pays.contains("issuer"))
        {
            if (!taker_pays.at("issuer").is_string())
                return {Error::rpcINVALID_PARAMS, "takerPaysIssuerNotString"};

            if (!ripple::to_issuer(
                    pay_issuer, taker_pays.at("issuer").as_string().c_str()))
                return {Error::rpcINVALID_PARAMS, "badTakerPaysIssuer"};

            if (pay_issuer == ripple::noAccount())
                return {Error::rpcINVALID_PARAMS, "badTakerPaysIssuerAccountOne"};
        }
        else
        {
            pay_issuer = ripple::xrpAccount();
        }

        if (isXRP(pay_currency) && !isXRP(pay_issuer))
            return {Error::rpcINVALID_PARAMS, 
                "Unneeded field 'taker_pays.issuer' for XRP currency "
                "specification."};

        if (!isXRP(pay_currency) && isXRP(pay_issuer))
            return {Error::rpcINVALID_PARAMS,
                    "Invalid field 'taker_pays.issuer', expected non-XRP "
                    "issuer."};

        ripple::AccountID get_issuer;

        if (taker_gets.contains("issuer"))
        {
            if (!taker_gets["issuer"].is_string())
                return {Error::rpcINVALID_PARAMS, 
                        "taker_gets.issuer should be string"};

            if (!ripple::to_issuer(
                    get_issuer, taker_gets.at("issuer").as_string().c_str()))
                return {Error::rpcINVALID_PARAMS,
                        "Invalid field 'taker_gets.issuer', bad issuer."};

            if (get_issuer == ripple::noAccount())
                return {Error::rpcINVALID_PARAMS,
                    "Invalid field 'taker_gets.issuer', bad issuer account "
                    "one."};
        }
        else
        {
            get_issuer = ripple::xrpAccount();
        }

        if (ripple::isXRP(get_currency) && !ripple::isXRP(get_issuer))
            return {Error::rpcINVALID_PARAMS,
                    "Unneeded field 'taker_gets.issuer' for XRP currency "
                    "specification."};

        if (!ripple::isXRP(get_currency) && ripple::isXRP(get_issuer))
            return {Error::rpcINVALID_PARAMS,
                "Invalid field 'taker_gets.issuer', expected non-XRP issuer."};

        if (pay_currency == get_currency && pay_issuer == get_issuer)
            return {Error::rpcINVALID_PARAMS, "badMarket"};

        ripple::Book book = {
            {pay_currency, pay_issuer}, {get_currency, get_issuer}};

        bookBase = getBookBase(book);
    }

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if(!request.at("limit").is_int64())
            return {Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return {Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }


    std::optional<ripple::AccountID> takerID = {};
    if (request.contains("taker"))
    {
        if (!request.at("taker").is_string())
            return {Error::rpcINVALID_PARAMS, "takerNotString"};

        takerID = 
            accountFromStringStrict(request.at("taker").as_string().c_str());
            
        if (!takerID)
            return {Error::rpcINVALID_PARAMS, "invalidTakerAccount"};
    }

    ripple::uint256 cursor = beast::zero;
    if (request.contains("cursor"))
    {
        if(!request.at("cursor").is_string())
            return {Error::rpcINVALID_PARAMS, "cursorNotString"};

        if (!cursor.parseHex(request.at("cursor").as_string().c_str()))
            return {Error::rpcINVALID_PARAMS, "malformedCursor"};
    }

    auto start = std::chrono::system_clock::now();
    auto [offers, retCursor, warning] =
        context_.backend->fetchBookOffers(bookBase, lgrInfo.seq, limit, cursor);
    auto end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning) << "Time loading books: "
                               << ((end - start).count() / 1000000000.0);



    response_["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response_["ledger_index"] = lgrInfo.seq;

    response_["offers"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonOffers = response_.at("offers").as_array();

    start = std::chrono::system_clock::now();
    for (auto const& obj : offers)
    {
        if (jsonOffers.size() == limit)
            break;

        try
        {
            ripple::SerialIter it{obj.blob.data(), obj.blob.size()};
            ripple::SLE offer{it, obj.key};
            ripple::uint256 bookDir = offer.getFieldH256(ripple::sfBookDirectory);

            boost::json::object offerJson = toJson(offer);
            offerJson["quality"] = ripple::amountFromQuality(getQuality(bookDir)).getText();
            jsonOffers.push_back(offerJson);
        }
        catch (std::exception const& e) {}
    }

    end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning) << "Time transforming to json: "
                               << ((end - start).count() / 1000000000.0);

    if (retCursor)
        response_["marker"] = ripple::strHex(*retCursor);
    if (warning)
        response_["warning"] =
            "Periodic database update in progress. Data for this book as of "
            "this ledger "
            "may be incomplete. Data should be complete within one minute";

    return OK;
}

} // namespace RPC