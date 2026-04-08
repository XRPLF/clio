#include "etl/Source.hpp"

#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/impl/ForwardingSource.hpp"
#include "etl/impl/GrpcSource.hpp"
#include "etl/impl/SourceImpl.hpp"
#include "etl/impl/SubscriptionSource.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/config/ObjectView.hpp"

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace etl {

SourcePtr
makeSource(
    util::config::ObjectView const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    std::chrono::steady_clock::duration forwardingTimeout,
    SourceBase::OnConnectHook onConnect,
    SourceBase::OnDisconnectHook onDisconnect,
    SourceBase::OnLedgerClosedHook onLedgerClosed
)
{
    auto const ip = config.get<std::string>("ip");
    auto const wsPort = config.get<std::string>("ws_port");
    auto const grpcPort = config.get<std::string>("grpc_port");

    impl::ForwardingSource forwardingSource{ip, wsPort, forwardingTimeout};
    impl::GrpcSource grpcSource{ip, grpcPort};
    auto subscriptionSource = std::make_unique<impl::SubscriptionSource>(
        ioc,
        ip,
        wsPort,
        std::move(validatedLedgers),
        std::move(subscriptions),
        std::move(onConnect),
        std::move(onDisconnect),
        std::move(onLedgerClosed)
    );

    return std::make_unique<impl::SourceImpl<>>(
        ip,
        wsPort,
        grpcPort,
        std::move(grpcSource),
        std::move(subscriptionSource),
        std::move(forwardingSource)
    );
}

}  // namespace etl
