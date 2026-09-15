/** @file */
#pragma once

#include "rpc/common/AnyHandler.hpp"

#include <memory>
#include <span>
#include <string_view>

namespace data {
class AmendmentCenterInterface;
class BackendInterface;
}  // namespace data

namespace etl {
struct ETLServiceInterface;
class LoadBalancerInterface;
}  // namespace etl

namespace feed {
class SubscriptionManagerInterface;
}  // namespace feed

namespace util::config {
class ClioConfigDefinition;
}  // namespace util::config

namespace rpc {
class Counters;
}  // namespace rpc

namespace rpc::impl {

/**
 * @brief The bundle of runtime dependencies a handler factory may consume.
 *
 * A short-lived view over the caller's objects: it holds references only, so it must
 * not outlive the call it is passed to.
 */
struct HandlerDeps {
    util::config::ClioConfigDefinition const& config;
    std::shared_ptr<data::BackendInterface> const& backend;
    std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptionManager;
    std::shared_ptr<etl::LoadBalancerInterface> const& balancer;
    std::shared_ptr<etl::ETLServiceInterface const> const& etl;
    std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter;
    Counters const& counters;
};

/**
 * @brief Constructs one type-erased handler from the available dependencies.
 *
 * A plain function pointer rather than @c std::function so that the registry stays a
 * literal type and is constant-initialised.
 */
using HandlerFactory = AnyHandler (*)(HandlerDeps const&);

/**
 * @brief One row of the handler registry: the method name, how to construct the
 * handler, and its static metadata.
 */
struct HandlerEntry {
    std::string_view name;
    HandlerFactory factory;
    bool isClioOnly = false;
};

/**
 * @brief The full set of registered RPC handlers.
 *
 * Static data: reading the registry constructs no handler and requires no runtime
 * dependencies.
 *
 * @return A span over the registry, valid for the lifetime of the program
 */
[[nodiscard]] std::span<HandlerEntry const>
handlerRegistry() noexcept;

}  // namespace rpc::impl
