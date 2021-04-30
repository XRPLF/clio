#include <boost/json.hpp>
#include <reporting/server/session.h>

static std::unordered_set<std::string> validStreams { 
    "ledger",
    "transactions" };

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
    std::shared_ptr<session>& session,
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
        else
            assert(false);
    }
}

void
unsubscribeToStreams(
    boost::json::object const& request,
    std::shared_ptr<session>& session,
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
        else
            assert(false);
    }
}

boost::json::value
validateAccounts(boost::json::object const& request)
{
    if (!request.at("accounts").is_array())
    {
        return "accounts must be array";
    }

    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        if (!account.is_string())
        {
            return "account must be strings";
        }

        std::string s = account.as_string().c_str();
        auto id = ripple::parseBase58<ripple::AccountID>(s);

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
    std::shared_ptr<session>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = ripple::parseBase58<ripple::AccountID>(s);

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
    std::shared_ptr<session>& session,
    SubscriptionManager& manager)
{
    boost::json::array const& accounts = request.at("accounts").as_array();

    for (auto const& account : accounts)
    {
        std::string s = account.as_string().c_str();

        auto accountID = ripple::parseBase58<ripple::AccountID>(s);

        if(!accountID)
        {
            assert(false);
            continue;
        }

        manager.unsubAccount(*accountID, session);
    }
}

boost::json::object
doSubscribe(
    boost::json::object const& request,
    std::shared_ptr<session>& session,
    SubscriptionManager& manager)
{
    boost::json::object response;

    if (request.contains("streams"))
    {
        boost::json::value error = validateStreams(request);

        if(!error.is_null())
        {
            response["error"] = error;
            return response;
        }   
    }

    if (request.contains("accounts"))
    {
        boost::json::value error = validateAccounts(request);

        if(!error.is_null())
        {
            response["error"] = error;
            return response;
        }  
    }

    if (request.contains("streams"))
        subscribeToStreams(request, session, manager);

    if (request.contains("accounts"))
        subscribeToAccounts(request, session, manager);

    response["status"] = "success";
    return response;
}

boost::json::object
doUnsubscribe(
    boost::json::object const& request,
    std::shared_ptr<session>& session,
    SubscriptionManager& manager)
{
    boost::json::object response;

    if (request.contains("streams"))
    {
        boost::json::value error = validateStreams(request);

        if(!error.is_null())
        {
            response["error"] = error;
            return response;
        }   
    }

    if (request.contains("accounts"))
    {
        boost::json::value error = validateAccounts(request);

        if(!error.is_null())
        {
            response["error"] = error;
            return response;
        }  
    }

    if (request.contains("streams"))
        unsubscribeToStreams(request, session, manager);

    if (request.contains("accounts"))
        unsubscribeToAccounts(request, session, manager);

    response["status"] = "success";
    return response;
}