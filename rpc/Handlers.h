#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include <reporting/ReportingETL.h>
#include <reporting/server/WsBase.h>

#include <unordered_map>
#include <iostream>

#ifndef RIPPLE_REPORTING_HANDLERS_H
#define RIPPLE_REPORTING_HANDLERS_H

class ReportingETL;
class SubscriptionManager;
class WsSession;

//------------------------------------------------------------------------------

namespace RPC {
namespace method {
#define METHOD(x) constexpr char[] x(#x)

METHOD(tx);
METHOD(account_tx);
METHOD(ledger);
METHOD(account_info);
METHOD(ledger_data);
METHOD(book_offers);
METHOD(ledger_range);
METHOD(ledger_entry);
METHOD(account_channels);
METHOD(account_lines);
METHOD(account_currencies);
METHOD(account_offers);
METHOD(account_objects);
METHOD(channel_authorize);
METHOD(channel_verify);
METHOD(subscribe);
METHOD(unsubscribe);
METHOD(submit);
METHOD(submit_multisigned);
METHOD(fee);
METHOD(path_find);
METHOD(ripple_path_find);
METHOD(manifest);

}
}

static std::unordered_set<std::string> forwardCommands{
    "submit",
    "submit_multisigned",
    "fee",
    "path_find",
    "ripple_path_find",
    "manifest"
};

static std::unordered_map<std::string, RPCCommand> commandMap{
    {"tx", tx},
    {"account_tx", account_tx},
    {"ledger", ledger},
    {"ledger_range", ledger_range},
    {"ledger_entry", ledger_entry},
    {"account_info", account_info},
    {"ledger_data", ledger_data},
    {"book_offers", book_offers},
    {"account_channels", account_channels},
    {"account_lines", account_lines},
    {"account_currencies", account_currencies},
    {"account_offers", account_offers},
    {"account_objects", account_objects},
    {"channel_authorize", channel_authorize},
    {"channel_verify", channel_verify},
    {"subscribe", subscribe},
    {"unsubscribe", unsubscribe}};

static std::unordered_map<std::string, std::function<void()> commands

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
doLedgerEntry(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedger(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedgerRange(
    boost::json::object const& request,
    BackendInterface const& backend);

boost::json::object
doAccountInfo(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountChannels(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountLines(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountCurrencies(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountOffers(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountObjects(
    boost::json::object const& request,
    BackendInterface const& backend);

boost::json::object
doChannelAuthorize(boost::json::object const& request);
boost::json::object
doChannelVerify(boost::json::object const& request);

boost::json::object
doSubscribe(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager);
boost::json::object
doUnsubscribe(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager);

extern boost::json::object
buildResponse(
    boost::json::object const& request,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> manager,
    std::shared_ptr<ETLLoadBalancer> balancer,
    std::shared_ptr<WsBase> session);

#endif // RIPPLE_REPORTING_HANDLERS_H
