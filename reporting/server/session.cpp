#include <reporting/server/session.h>

void
fail(boost::beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

boost::json::object
buildResponse(
    boost::json::object const& request,
    BackendInterface const& backend,
    SubscriptionManager& subManager,
    std::shared_ptr<session> session)
{
    std::string command = request.at("command").as_string().c_str();
    boost::json::object response;
    switch (commandMap[command])
    {
        case tx:
            return doTx(request, backend);
            break;
        case account_tx:
            return doAccountTx(request, backend);
            break;
        case book_offers:
            return doBookOffers(request, backend);
            break;
        case ledger:
            return doLedger(request, backend);
            break;
        case ledger_data:
            return doLedgerData(request, backend);
            break;
        case account_info:
            return doAccountInfo(request, backend);
            break;
        case subscribe:
            return doSubscribe(request, session, subManager);
            break;
        case unsubscribe:
            return doUnsubscribe(request, session, subManager);
            break;
        default:
            BOOST_LOG_TRIVIAL(error) << "Unknown command: " << command;
    }
    return response;
}