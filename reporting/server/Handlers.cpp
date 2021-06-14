#include <reporting/server/Handlers.h>

extern boost::json::object
buildResponse(
    boost::json::object const& request,
    ReportingETL& etl,
    std::shared_ptr<WsSession> session)
{
    std::string command = request.at("command").as_string().c_str();
    BOOST_LOG_TRIVIAL(info) << "Received rpc command : " << request;
    boost::json::object response;

    BackendInterface& backend = etl.getFlatMapBackend();
    SubscriptionManager& manager = etl.getSubscriptionManager();
    switch (commandMap[command])
    {
        case tx:
            return doTx(request, backend);
        case account_tx:
            return doAccountTx(request, backend);
        case ledger:
            return doLedger(request, backend);
        case ledger_entry:
            return doLedgerEntry(request, backend);
        case ledger_range:
            return doLedgerRange(request, backend);
        case ledger_data:
            return doLedgerData(request, backend);
        case account_info:
            return doAccountInfo(request, backend);
        case book_offers:
            return doBookOffers(request, backend);
        case account_channels:
            return doAccountChannels(request, backend);
        case account_lines:
            return doAccountLines(request, backend);
        case account_currencies:
            return doAccountCurrencies(request, backend);
        case account_offers:
            return doAccountOffers(request, backend);
        case account_objects:
            return doAccountObjects(request, backend);
        case channel_authorize:
            return doChannelAuthorize(request);
        case channel_verify:
            return doChannelVerify(request);
        case subscribe:
            return doSubscribe(request, session, manager);
        case unsubscribe:
            return doUnsubscribe(request, session, manager);
        default:
            BOOST_LOG_TRIVIAL(error) << "Unknown command: " << command;
    }
    return response;
}