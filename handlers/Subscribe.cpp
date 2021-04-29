#include <boost/json.hpp>
#include <reporting/server/session.h>

static std::unordered_set<std::string> validStreams { 
    "ledger",
    "transactions" };

boost::json::object
doSubscribe(
    boost::json::object const& request,
    std::shared_ptr<session>& session,
    SubscriptionManager& manager)
{
    boost::json::object response;

    if (!request.contains("streams") || !request.at("streams").is_array())
    {
        response["error"] = "missing or invalid streams";
        return response;
    }

    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        if (!stream.is_string())
        {
            response["error"] = "streams must be strings";
            return response;
        }

        std::string s = stream.as_string().c_str();

        if (validStreams.find(s) == validStreams.end())
        {
            response["error"] = "invalid stream " + s;
            return response;
        }
    }

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

    if (!request.contains("streams") || !request.at("streams").is_array())
    {
        response["error"] = "missing or invalid streams";
        return response;
    }

    boost::json::array const& streams = request.at("streams").as_array();

    for (auto const& stream : streams)
    {
        if (!stream.is_string())
        {
            response["error"] = "streams must be strings";
            return response;
        }

        std::string s = stream.as_string().c_str();

        if (validStreams.find(s) == validStreams.end())
        {
            response["error"] = "invalid stream " + s;
            return response;
        }
    }

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

    response["status"] = "success";
    return response;
}