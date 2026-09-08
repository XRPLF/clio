#include "rpc/common/impl/HandlerProvider.hpp"

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "etl/ETLServiceInterface.hpp"
#include "etl/LoadBalancerInterface.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/impl/HandlerRegistry.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace rpc::impl {

ProductionHandlerProvider::ProductionHandlerProvider(
    util::config::ClioConfigDefinition const& config,
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptionManager,
    std::shared_ptr<etl::LoadBalancerInterface> const& balancer,
    std::shared_ptr<etl::ETLServiceInterface const> const& etl,
    std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter,
    Counters const& counters
)
{
    HandlerDeps const deps{
        .config = config,
        .backend = backend,
        .subscriptionManager = subscriptionManager,
        .balancer = balancer,
        .etl = etl,
        .amendmentCenter = amendmentCenter,
        .counters = counters
    };

    auto const registry = handlerRegistry();
    handlerMap_.reserve(registry.size());

    for (auto const& entry : registry) {
        handlerMap_.emplace(
            entry.name,
            Handler{
                .handler = entry.factory(deps),
                .isClioOnly = entry.isClioOnly,
            }
        );
    }
}

bool
ProductionHandlerProvider::contains(std::string const& command) const
{
    return handlerMap_.contains(command);
}

std::optional<AnyHandler>
ProductionHandlerProvider::getHandler(std::string const& command) const
{
    if (!handlerMap_.contains(command))
        return {};

    return handlerMap_.at(command).handler;
}

bool
ProductionHandlerProvider::isClioOnly(std::string const& command) const
{
    return handlerMap_.contains(command) && handlerMap_.at(command).isClioOnly;
}

std::unordered_set<std::string>
ProductionHandlerProvider::handlerNames() const
{
    std::unordered_set<std::string> result;
    for (auto const& [name, handler] : handlerMap_)
        result.insert(name);
    return result;
}

}  // namespace rpc::impl
