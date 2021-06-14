#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include <reporting/ReportingETL.h>

#include <unordered_map>
#include <iostream>

#ifndef RIPPLE_REPORTING_HANDLERS_H
#define RIPPLE_REPORTING_HANDLERS_H

class ReportingETL;
class SubscriptionManager;
class WsSession;

static enum RPCCommand { tx, account_tx, ledger, account_info, book_offers, ledger_data, subscribe, unsubscribe };
static std::unordered_map<std::string, RPCCommand> commandMap{
        {"tx", tx},
        {"account_tx", account_tx},
        {"ledger", ledger},
        {"account_info", account_info},
        {"book_offers", book_offers},
        {"ledger_data", ledger_data},
        {"subscribe", subscribe},
        {"unsubscribe", unsubscribe}};

boost::json::object
doAccountInfo(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doTx(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountTx(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doBookOffers(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedgerData(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedger(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doSubscribe(
    boost::json::object const& request,
    std::shared_ptr<WsSession>& session,
    SubscriptionManager& manager);
boost::json::object
doUnsubscribe(
    boost::json::object const& request,
    std::shared_ptr<WsSession>& session,
    SubscriptionManager& manager);

extern boost::json::object
buildResponse(
    boost::json::object const& request,
    ReportingETL& etl,
    std::shared_ptr<WsSession> session);

#endif // RIPPLE_REPORTING_HANDLERS_H
