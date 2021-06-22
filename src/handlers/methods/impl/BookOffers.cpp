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
        if (!request.contains("taker_pays"))
            return Status{Error::rpcINVALID_PARAMS, "missingTakerPays"};

        if (!request.contains("taker_gets"))
            return Status{Error::rpcINVALID_PARAMS, "missingTakerGets"};

        if (!request.at("taker_pays").is_object())
            return Status{Error::rpcINVALID_PARAMS, "takerPaysNotObject"};
        
        if (!request.at("taker_gets").is_object())
            return Status{Error::rpcINVALID_PARAMS, "takerGetsNotObject"};

        auto taker_pays = request.at("taker_pays").as_object();
        if (!taker_pays.contains("currency"))
            return Status{Error::rpcINVALID_PARAMS, "missingTakerPaysCurrency"};

        if (!taker_pays.at("currency").is_string())
            return Status{Error::rpcINVALID_PARAMS, "takerPaysCurrencyNotString"};

        auto taker_gets = request.at("taker_gets").as_object();
        if (!taker_gets.contains("currency"))
            return Status{Error::rpcINVALID_PARAMS, "missingTakerGetsCurrency"};

        if (!taker_gets.at("currency").is_string())
            return Status{Error::rpcINVALID_PARAMS, "takerGetsCurrencyNotString"};

        ripple::Currency pay_currency;
        if (!ripple::to_currency(
                pay_currency, taker_pays.at("currency").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "badTakerPaysCurrency"};

        ripple::Currency get_currency;
        if (!ripple::to_currency(
                get_currency, taker_gets["currency"].as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "badTakerGetsCurrency"};

        ripple::AccountID pay_issuer;
        if (taker_pays.contains("issuer"))
        {
            if (!taker_pays.at("issuer").is_string())
                return Status{Error::rpcINVALID_PARAMS, "takerPaysIssuerNotString"};

            if (!ripple::to_issuer(
                    pay_issuer, taker_pays.at("issuer").as_string().c_str()))
                return Status{Error::rpcINVALID_PARAMS, "badTakerPaysIssuer"};

            if (pay_issuer == ripple::noAccount())
                return Status{Error::rpcINVALID_PARAMS, "badTakerPaysIssuerAccountOne"};
        }
        else
        {
            pay_issuer = ripple::xrpAccount();
        }

        if (isXRP(pay_currency) && !isXRP(pay_issuer))
            return Status{Error::rpcINVALID_PARAMS, 
                "Unneeded field 'taker_pays.issuer' for XRP currency "
                "specification."};

        if (!isXRP(pay_currency) && isXRP(pay_issuer))
            return Status{Error::rpcINVALID_PARAMS,
                    "Invalid field 'taker_pays.issuer', expected non-XRP "
                    "issuer."};

        ripple::AccountID get_issuer;

        if (taker_gets.contains("issuer"))
        {
            if (!taker_gets["issuer"].is_string())
                return Status{Error::rpcINVALID_PARAMS, 
                        "taker_gets.issuer should be string"};

            if (!ripple::to_issuer(
                    get_issuer, taker_gets.at("issuer").as_string().c_str()))
                return Status{Error::rpcINVALID_PARAMS,
                        "Invalid field 'taker_gets.issuer', bad issuer."};

            if (get_issuer == ripple::noAccount())
                return Status{Error::rpcINVALID_PARAMS,
                    "Invalid field 'taker_gets.issuer', bad issuer account "
                    "one."};
        }
        else
        {
            get_issuer = ripple::xrpAccount();
        }

        if (ripple::isXRP(get_currency) && !ripple::isXRP(get_issuer))
            return Status{Error::rpcINVALID_PARAMS,
                    "Unneeded field 'taker_gets.issuer' for XRP currency "
                    "specification."};

        if (!ripple::isXRP(get_currency) && ripple::isXRP(get_issuer))
            return Status{Error::rpcINVALID_PARAMS,
                "Invalid field 'taker_gets.issuer', expected non-XRP issuer."};

        if (pay_currency == get_currency && pay_issuer == get_issuer)
            return Status{Error::rpcINVALID_PARAMS, "badMarket"};

        book = {{pay_currency, pay_issuer}, {get_currency, get_issuer}};
        bookBase = getBookBase(book);
    }

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if(!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }


    std::optional<ripple::AccountID> takerID = {};
    if (request.contains("taker"))
    {
        if (!request.at("taker").is_string())
            return Status{Error::rpcINVALID_PARAMS, "takerNotString"};

        takerID = 
            accountFromStringStrict(request.at("taker").as_string().c_str());
            
        if (!takerID)
            return Status{Error::rpcINVALID_PARAMS, "invalidTakerAccount"};
    }

    ripple::uint256 cursor = beast::zero;
    if (request.contains("cursor"))
    {
        if(!request.at("cursor").is_string())
            return Status{Error::rpcINVALID_PARAMS, "cursorNotString"};

        if (!cursor.parseHex(request.at("cursor").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedCursor"};
    }

    auto start = std::chrono::system_clock::now();
    auto [offers, retCursor, warning] =
        context.backend->fetchBookOffers(bookBase, lgrInfo.seq, limit, cursor);
    auto end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning) << "Time loading books: "
                               << ((end - start).count() / 1000000000.0);

    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

    response["offers"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonOffers = response.at("offers").as_array();

    std::map<ripple::AccountID, ripple::STAmount> umBalance;

    bool globalFreeze = 
        isGlobalFrozen(*context.backend, lgrInfo.seq, book.out.account) ||
        isGlobalFrozen(*context.backend, lgrInfo.seq, book.out.account);

    auto rate = transferRate(*context.backend, lgrInfo.seq, book.out.account);

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

            auto const uOfferOwnerID = offer.getAccountID(ripple::sfAccount);
            auto const& saTakerGets = offer.getFieldAmount(ripple::sfTakerGets);
            auto const& saTakerPays = offer.getFieldAmount(ripple::sfTakerPays);
            ripple::STAmount saOwnerFunds;
            bool firstOwnerOffer = true;

            if (book.out.account == uOfferOwnerID)
            {
                // If an offer is selling issuer's own IOUs, it is fully
                // funded.
                saOwnerFunds = saTakerGets;
            }
            else if (globalFreeze)
            {
                // If either asset is globally frozen, consider all offers
                // that aren't ours to be totally unfunded
                saOwnerFunds.clear(book.out);
            }
            else
            {
                auto umBalanceEntry = umBalance.find(uOfferOwnerID);
                if (umBalanceEntry != umBalance.end())
                {
                    // Found in running balance table.

                    saOwnerFunds = umBalanceEntry->second;
                    firstOwnerOffer = false;
                }
                else {
                    saOwnerFunds = accountHolds(
                        *context.backend,
                        lgrInfo.seq,
                        uOfferOwnerID,
                        book.out.currency,
                        book.out.account);

                    if (saOwnerFunds < beast::zero)
                        saOwnerFunds.clear();
                }
            }

            boost::json::object offerJson = toJson(offer);

            ripple::STAmount saTakerGetsFunded;
            ripple::STAmount saOwnerFundsLimit = saOwnerFunds;
            ripple::Rate offerRate = ripple::parityRate;
            ripple::STAmount dirRate =
                ripple::amountFromQuality(getQuality(bookDir));

            if (rate != ripple::parityRate
                // Have a tranfer fee.
                && takerID != book.out.account
                // Not taking offers of own IOUs.
                && book.out.account != uOfferOwnerID)
            // Offer owner not issuing ownfunds
            {
                // Need to charge a transfer fee to offer owner.
                offerRate = rate;
                saOwnerFundsLimit = ripple::divide(saOwnerFunds, offerRate);
            }

            if (saOwnerFundsLimit >= saTakerGets)
            {
                // Sufficient funds no shenanigans.
                saTakerGetsFunded = saTakerGets;
            }
            else
            {
                saTakerGetsFunded = saOwnerFundsLimit;
                offerJson["taker_gets_funded"] = saTakerGetsFunded.getText();
                offerJson["taker_pays_funded"] = toBoostJson(std::min(
                    saTakerPays,
                    ripple::multiply(
                        saTakerGetsFunded, dirRate, saTakerPays.issue()))
                    .getJson(ripple::JsonOptions::none));
            }

            ripple::STAmount saOwnerPays = (ripple::parityRate == offerRate)
                ? saTakerGetsFunded
                : std::min(
                        saOwnerFunds, 
                        ripple::multiply(saTakerGetsFunded, offerRate));
            
            umBalance[uOfferOwnerID] = saOwnerFunds - saOwnerPays;

            if (firstOwnerOffer)
                offerJson["owner_funds"] = saOwnerFunds.getText();

            offerJson["quality"] = dirRate.getText();

            jsonOffers.push_back(offerJson);
        }
        catch (std::exception const& e) {}
    }

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

} // namespace RPC