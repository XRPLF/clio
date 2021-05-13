#include <reporting/server/session.h>
#include <reporting/P2pProxy.h>

void
fail(boost::beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

boost::json::object
buildResponse(
    boost::json::object const& request,
    ReportingETL& etl,
    std::shared_ptr<session> session)
{
    std::string command = request.at("command").as_string().c_str();
    boost::json::object response;

    if (shouldForwardToP2p(request))
        return etl.getETLLoadBalancer().forwardToP2p(request);

    switch (commandMap[command])
    {
        case tx:
            return doTx(request, etl.getFlatMapBackend());
        case account_tx:
            return doAccountTx(request, etl.getFlatMapBackend());
        case book_offers:
            return doBookOffers(request, etl.getFlatMapBackend());
        case ledger:
            return doLedger(request, backend);
            break;
        case ledger_data:
            return doLedgerData(request, etl.getFlatMapBackend());
        case account_info:
            return doAccountInfo(request, etl.getFlatMapBackend());
        case subscribe:
            return doSubscribe(request, session, etl.getSubscriptionManager());
        case unsubscribe:
            return doUnsubscribe(request, session, etl.getSubscriptionManager());
        default:
            response["error"] = "Unknown command: " + command;
            return response;
    }
}