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
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> manager,
    std::shared_ptr<ETLLoadBalancer> balancer,
    std::shared_ptr<session> session)
{
    std::string command = request.at("command").as_string().c_str();
    boost::json::object response;

    if (shouldForwardToP2p(request))
        return balancer->forwardToP2p(request);

    switch (commandMap[command])
    {
        case tx:
            return doTx(request, *backend);
        case account_tx:
            return doAccountTx(request, *backend);
        case book_offers:
            return doBookOffers(request, *backend);
        case ledger:
            return doLedger(request, *backend);
        case ledger_data:
            return doLedgerData(request, *backend);
        case account_info:
            return doAccountInfo(request, *backend);
        case subscribe:
            return doSubscribe(request, session, *manager);
        case unsubscribe:
            return doUnsubscribe(request, session, *manager);
        default:
            response["error"] = "Unknown command: " + command;
            return response;
    }
}