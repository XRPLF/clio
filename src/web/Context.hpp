#pragma once

#include "data/Types.hpp"
#include "util/Taggable.hpp"
#include "util/log/Logger.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace web {

/**
 * @brief Context that is used by the Webserver to pass around information about an incoming
 * request.
 */
struct Context : util::Taggable {
    boost::asio::yield_context yield;
    std::string method;
    std::uint32_t apiVersion;
    boost::json::object params;
    SubscriptionContextPtr session;
    data::LedgerRange range;
    std::string clientIp;
    bool isAdmin;

    /**
     * @brief Create a new Context instance.
     *
     * @param yield The coroutine context
     * @param command The method/command requested
     * @param apiVersion The api_version parsed from the request
     * @param params Request's parameters/data as a JSON object
     * @param subscriptionContext The subscription context of the connection
     * @param tagFactory A factory that is used to generate tags to track requests and connections
     * @param range The ledger range that is available at the time of the request
     * @param clientIp IP of the peer
     * @param isAdmin Whether the peer has admin privileges
     */
    Context(
        boost::asio::yield_context yield,
        std::string command,
        std::uint32_t apiVersion,
        boost::json::object params,
        SubscriptionContextPtr subscriptionContext,
        util::TagDecoratorFactory const& tagFactory,
        data::LedgerRange const& range,
        std::string clientIp,
        bool isAdmin
    )
        : Taggable(tagFactory)
        , yield(std::move(yield))
        , method(std::move(command))
        , apiVersion(apiVersion)
        , params(std::move(params))
        , session(std::move(subscriptionContext))
        , range(range)
        , clientIp(std::move(clientIp))
        , isAdmin(isAdmin)
    {
        static util::Logger const log{"Performance"};  // NOLINT(readability-identifier-naming)
        LOG(log.debug()) << tag() << "new Context created";
    }

    Context(Context&&) = default;
    Context&
    operator=(Context&&) = default;
};

}  // namespace web
