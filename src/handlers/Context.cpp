#include <handlers/Context.h>

namespace RPC
{

std::optional<Context>
make_WsContext(
    boost::json::object const& request,
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    std::shared_ptr<WsBase> const& session,
    Backend::LedgerRange const& range)
{
    if (!request.contains("command"))
        return {};
    
    std::string command = request.at("command").as_string().c_str();
    return Context{
        command,
        1,
        request,
        backend,
        subscriptions,
        balancer,
        session,
        range
    };
}

std::optional<Context>
make_HttpContext(
    boost::json::object const& request,
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    Backend::LedgerRange const& range)
{
    if (!request.contains("method") || !request.at("method").is_string())
        return {};

    std::string const& command = request.at("method").as_string().c_str();

    // empty params
    if (!request.contains("params") || !request.at("params").is_array())
        return {};

    boost::json::array const& array = request.at("params").as_array();

    if (array.size() != 1)
        return {};
    
    if (!array.at(0).is_object())
        return {};
    
    return Context{
        command,
        1,
        array.at(0).as_object(),
        backend,
        subscriptions,
        balancer,
        nullptr,
        range
    };
}
    
} // namespace RPC