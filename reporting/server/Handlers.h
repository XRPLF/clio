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
    std::shared_ptr<session>& session,
    SubscriptionManager& manager);
boost::json::object
doUnsubscribe(
    boost::json::object const& request,
    std::shared_ptr<session>& session,
    SubscriptionManager& manager);

boost::json::object
buildResponse(
    boost::json::object const& request,
    BackendInterface const& backend,
    SubscriptionManager& subManager,
    std::shared_ptr<session> session);

void
fail(boost::beast::error_code ec, char const* what);