#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <handlers/RPCHelpers.h>
#include <reporting/DBHelpers.h>
#include <reporting/Pg.h>
#include <reporting/ReportingBackend.h>

std::optional<std::uint32_t>
ledgerSequenceFromRequest(
    boost::json::object const& request,
    std::shared_ptr<PgPool> const& pool)
{
    std::stringstream sql;
    sql << "SELECT ledger_seq FROM ledgers WHERE ";

    if (request.contains("ledger_index"))
    {
        sql << "ledger_seq = "
            << std::to_string(request.at("ledger_index").as_int64());
    }
    else if (request.contains("ledger_hash"))
    {
        sql << "ledger_hash = \\\\x" << request.at("ledger_hash").as_string();
    }
    else
    {
        sql.str("");
        sql << "SELECT max_ledger()";
    }

    sql << ";";

    auto index = PgQuery(pool)(sql.str().c_str());
    if (!index || index.isNull())
        return {};

    return std::optional<std::uint32_t>{index.asInt()};
}

std::vector<ripple::uint256>
loadBookOfferIndexes(
    ripple::Book const& book,
    std::uint32_t seq,
    std::uint32_t limit,
    std::shared_ptr<PgPool> const& pool)
{
    std::vector<ripple::uint256> hashes = {};

    ripple::uint256 bookBase = getBookBase(book);
    ripple::uint256 bookEnd = getQualityNext(bookBase);

    pg_params dbParams;

    char const*& command = dbParams.first;
    std::vector<std::optional<std::string>>& values = dbParams.second;

    command =
        "SELECT offer_indexes FROM books "
        "WHERE book_directory >= $1::bytea "
        "AND book_directory < $2::bytea "
        "AND ledger_index <= $3::bigint "
        "LIMIT $4::bigint";

    values.resize(4);
    values[0] = "\\x" + ripple::strHex(bookBase);
    values[1] = "\\x" + ripple::strHex(bookEnd);
    values[2] = std::to_string(seq);
    values[3] = std::to_string(limit);

    auto indexes = PgQuery(pool)(dbParams);
    if (!indexes || indexes.isNull())
        return {};

    for (auto i = 0; i < indexes.ntuples(); ++i)
    {
        auto unHexed = ripple::strUnHex(indexes.c_str(i) + 2);
        if (unHexed)
            hashes.push_back(ripple::uint256::fromVoid(unHexed->data()));
    }

    return hashes;
}

boost::json::object
doBookOffers(
    boost::json::object const& request,
    CassandraFlatMapBackend const& backend,
    std::shared_ptr<PgPool>& pool)
{
    std::cout << "enter" << std::endl;
    boost::json::object response;
    auto sequence = ledgerSequenceFromRequest(request, pool);

    if (!sequence)
        return response;

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

    std::uint32_t limit = 2048;
    if (request.contains("limit") and
        request.at("limit").kind() == boost::json::kind::int64)
        limit = request.at("limit").as_int64();

    ripple::Book book = {
        {pay_currency, pay_issuer}, {get_currency, get_issuer}};

    auto start = std::chrono::system_clock::now();
    ripple::uint256 bookBase = getBookBase(book);
    std::vector<CassandraFlatMapBackend::LedgerObject> offers =
        backend.doBookOffers(bookBase, *sequence);
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
                return getJson(offer);
            }
            catch (std::exception const& e)
            {
                boost::json::object empty;
                return empty;
            }
        });

    end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(warning) << "Time transforming to json: "
                               << ((end - start).count() / 1000000000.0);

    return response;
}
