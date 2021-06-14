#include <boost/json.hpp>
<<<<<<< HEAD
=======
#include <reporting/server/WsSession.h>
#include <reporting/server/SubscriptionManager.h>
>>>>>>> 27506bc (rebase handlers)
#include <handlers/RPCHelpers.h>
#include <server/session.h>

static std::unordered_set<std::string> validStreams{
    "ledger",
    "transactions",
    "transactions_proposed"};

boost::json::value
validateStreams(boost::json::object const& request)
{
    if (!request.at("streams").is_array())
    {
        return "missing or invalid streams";
    }

    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        if (!stream.is_string())
        {
            return "streams must be strings";
        }

        std::string s = stream.as_string().c_str();

        if (validStreams.find(s) == validStreams.end())
        {
            return boost::json::string("invalid stream " + s);
        }
    }

    return nullptr;
}

void
subscribeToStreams(
    boost::json::object const& request,
    std::shared_ptr<WsSession>& session,
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
    std::shared_ptr<WsSession>& session,
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

boost::json::value
validateAccounts(
    boost::json::object const& request,
    boost::json::array const& accounts)
{
    for (auto const& account : accounts)
    {
        if (!account.is_string())
        {
            return "account must be strings";
        }

        std::string s = account.as_string().c_str();
        auto id = accountFromStringStrict(s);

        if (!id)
        {
            return boost::json::string("invalid account " + s);
        }
    }

    return nullptr;
}

void
subscribeToAccounts(
    boost::json::object const& request,
    std::shared_ptr<WsSession>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = accountFromStringStrict(s);

        if (!accountID)
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
    std::shared_ptr<WsSession>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = accountFromStringStrict(s);

        if (!accountID)
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
    std::shared_ptr<session>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts =
        request.at("accounts_proposed").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = ripple::parseBase58<ripple::AccountID>(s);

        if (!accountID)
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
    std::shared_ptr<session>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts =
        request.at("accounts_proposed").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = ripple::parseBase58<ripple::AccountID>(s);

        if (!accountID)
        {
            assert(false);
            continue;
        }

        manager.unsubProposedAccount(*accountID, session);
    }
}

boost::json::object
doSubscribe(
    boost::json::object const& request,
    std::shared_ptr<WsSession>& session,
    SubscriptionManager& manager)
{
    boost::json::object response;

    if (request.contains("streams"))
    {
        boost::json::value error = validateStreams(request);

        if (!error.is_null())
        {
            response["error"] = error;
            return response;
        }
    }

    if (request.contains("accounts"))
    {
        if (!request.at("accounts").is_array())
        {
            response["error"] = "accounts must be array";
            return response;
        }

        boost::json::array accounts = request.at("accounts").as_array();
        boost::json::value error = validateAccounts(request, accounts);

        if (!error.is_null())
        {
            response["error"] = error;
            return response;
        }
    }

    if (request.contains("accounts_proposed"))
    {
        if (!request.at("accounts_proposed").is_array())
        {
            response["error"] = "accounts_proposed must be array";
            return response;
        }

        boost::json::array accounts =
            request.at("accounts_proposed").as_array();
        boost::json::value error = validateAccounts(request, accounts);

        if (!error.is_null())
        {
            response["error"] = error;
            return response;
        }
    }

    if (request.contains("streams"))
        subscribeToStreams(request, session, manager);

    if (request.contains("accounts"))
        subscribeToAccounts(request, session, manager);

    if (request.contains("accounts_proposed"))
        subscribeToAccountsProposed(request, session, manager);

    response["status"] = "success";
    return response;
}

boost::json::object
doUnsubscribe(
    boost::json::object const& request,
    std::shared_ptr<WsSession>& session,
    SubscriptionManager& manager)
{
    boost::json::object response;

    if (request.contains("streams"))
    {
        boost::json::value error = validateStreams(request);

        if (!error.is_null())
        {
            response["error"] = error;
            return response;
        }
    }

    if (request.contains("accounts"))
    {
        boost::json::array accounts = request.at("accounts").as_array();
        boost::json::value error = validateAccounts(request, accounts);

        if (!error.is_null())
        {
            response["error"] = error;
            return response;
        }
    }

    if (request.contains("accounts_proposed"))
    {
        boost::json::array accounts =
            request.at("accounts_proposed").as_array();
        boost::json::value error = validateAccounts(request, accounts);

        if (!error.is_null())
        {
            response["error"] = error;
            return response;
        }
    }

    if (request.contains("streams"))
        unsubscribeToStreams(request, session, manager);

    if (request.contains("accounts"))
        unsubscribeToAccounts(request, session, manager);

    if (request.contains("accounts_proposed"))
        unsubscribeToAccountsProposed(request, session, manager);

    response["status"] = "success";
    return response;
}
