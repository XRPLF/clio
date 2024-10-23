//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "etl/Source.hpp"

#include "data/BackendInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/impl/ForwardingSource.hpp"
#include "etl/impl/GrpcSource.hpp"
#include "etl/impl/SourceImpl.hpp"
#include "etl/impl/SubscriptionSource.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace etl {

SourcePtr
make_Source(
    util::config::ObjectView const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    std::chrono::steady_clock::duration forwardingTimeout,
    SourceBase::OnConnectHook onConnect,
    SourceBase::OnDisconnectHook onDisconnect,
    SourceBase::OnLedgerClosedHook onLedgerClosed
)
{
    auto const ip = config.getValue<std::string>("ip");
    auto const wsPort = config.getValue<std::string>("ws_port");
    auto const grpcPort = config.getValue<std::string>("grpc_port");

    impl::ForwardingSource forwardingSource{ip, wsPort, forwardingTimeout};
    impl::GrpcSource grpcSource{ip, grpcPort, std::move(backend)};
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
        ip, wsPort, grpcPort, std::move(grpcSource), std::move(subscriptionSource), std::move(forwardingSource)
    );
}

}  // namespace etl
