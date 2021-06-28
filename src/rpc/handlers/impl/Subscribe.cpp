#include <boost/json.hpp>
#include <server/WsBase.h>
#include <server/SubscriptionManager.h>
#include <rpc/handlers/Subscribe.h>
#include <rpc/RPCHelpers.h>

namespace RPC
{

static std::unordered_set<std::string> validStreams { 
    "ledger",
    "transactions",
    "transactions_proposed" };

Status
validateStreams(boost::json::object const& request)
{
    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        if (!stream.is_string())
            return {Error::rpcINVALID_PARAMS, "streamNotString"};

        std::string s = stream.as_string().c_str();

        if (validStreams.find(s) == validStreams.end())
            return {Error::rpcINVALID_PARAMS, "invalidStream" + s};
    }

    return OK;
}

void
subscribeToStreams(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        std::string s = stream.as_string().c_str();

        if (s == "ledger")
            manager.subLedger(session);
        else if (s == "transactions")
            manager.subTransactions(session);
        else if (s == "transactions_proposed")
            manager.subProposedTransactions(session);
        else
            assert(false);
    }
}

void
unsubscribeToStreams(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        std::string s = stream.as_string().c_str();

        if (s == "ledger")
            manager.unsubLedger(session);
        else if (s == "transactions")
            manager.unsubTransactions(session);
        else if (s == "transactions_proposed")
            manager.unsubProposedTransactions(session);
        else
            assert(false);
    }
}

Status
validateAccounts(boost::json::array const& accounts)
{
    for (auto const& account : accounts)
    {
        if (!account.is_string())
            return {Error::rpcINVALID_PARAMS, "accountNotString"};

        std::string s = account.as_string().c_str();
        auto id = accountFromStringStrict(s);

        if (!id)
            return {Error::rpcINVALID_PARAMS, "invalidAccount" + s};
    }

    return OK;
}

void
subscribeToAccounts(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = accountFromStringStrict(s);

        if(!accountID)
        {
            assert(false);
            continue;
        }

        manager.subAccount(*accountID, session);
    }
}

void
unsubscribeToAccounts(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = accountFromStringStrict(s);

        if(!accountID)
        {
            assert(false);
            continue;
        }

        manager.unsubAccount(*accountID, session);
    }
}

void
subscribeToAccountsProposed(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts_proposed").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = ripple::parseBase58<ripple::AccountID>(s);

        if(!accountID)
        {
            assert(false);
            continue;
        }

        manager.subProposedAccount(*accountID, session);
    }
}

void
unsubscribeToAccountsProposed(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts_proposed").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = ripple::parseBase58<ripple::AccountID>(s);

        if(!accountID)
        {
            assert(false);
            continue;
        }

        manager.unsubProposedAccount(*accountID, session);
    }
}


Status
Subscribe::check()
{
    auto request = context_.params;

    if (request.contains("streams"))
    {
        if (!request.at("streams").is_array())
            return {Error::rpcINVALID_PARAMS, "streamsNotArray"};

        auto status = validateStreams(request);

        if (status)
            return status;
    }

    if (request.contains("accounts"))
    {

        if (!request.at("accounts").is_array())
            return {Error::rpcINVALID_PARAMS, "accountsNotArray"};

        boost::json::array accounts = request.at("accounts").as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    if (request.contains("accounts_proposed"))
    {
        if (!request.at("accounts_proposed").is_array())
            return {Error::rpcINVALID_PARAMS, "accountsProposedNotArray"};

        boost::json::array accounts = request.at("accounts_proposed").as_array();
        auto status = validateAccounts(accounts);

        if(status)
            return status;
    }

    if (request.contains("streams"))
        subscribeToStreams(request, context_.session, *context_.subscriptions);

    if (request.contains("accounts"))
        subscribeToAccounts(request, context_.session, *context_.subscriptions);

    if (request.contains("accounts_proposed"))
        subscribeToAccountsProposed(request, context_.session, *context_.subscriptions);

    return OK;
}

Status
Unsubscribe::check()
{
    auto request = context_.params;

   if (request.contains("streams"))
    {
        if (!request.at("streams").is_array())
            return {Error::rpcINVALID_PARAMS, "streamsNotArray"};

        auto status = validateStreams(request);

        if (status)
            return status;
    }

    if (request.contains("accounts"))
    {

        if (!request.at("accounts").is_array())
            return {Error::rpcINVALID_PARAMS, "accountsNotArray"};

        boost::json::array accounts = request.at("accounts").as_array();
        auto status = validateAccounts(accounts);

        if (status)
            return status;
    }

    if (request.contains("accounts_proposed"))
    {
        if (!request.at("accounts_proposed").is_array())
            return {Error::rpcINVALID_PARAMS, "accountsProposedNotArray"};

        boost::json::array accounts = request.at("accounts_proposed").as_array();
        auto status = validateAccounts(accounts);

        if(status)
            return status;
    }

    if (request.contains("streams"))
        unsubscribeToStreams(request, context_.session, *context_.subscriptions);

    if (request.contains("accounts"))
        unsubscribeToAccounts(request, context_.session, *context_.subscriptions);

    if (request.contains("accounts_proposed"))
        unsubscribeToAccountsProposed(request, context_.session, *context_.subscriptions);

    return OK;
}

} // namespace RPC