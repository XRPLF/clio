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

#include "etlng/Source.hpp"

#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/impl/SubscriptionSource.hpp"
#include "etlng/impl/ForwardingSource.hpp"
#include "etlng/impl/GrpcSource.hpp"
#include "etlng/impl/SourceImpl.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/config/ObjectView.hpp"

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace etlng {

SourcePtr
makeSource(
    util::config::ObjectView const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<etl::NetworkValidatedLedgersInterface> validatedLedgers,
    std::chrono::steady_clock::duration forwardingTimeout,
    SourceBase::OnConnectHook onConnect,
    SourceBase::OnDisconnectHook onDisconnect,
    SourceBase::OnLedgerClosedHook onLedgerClosed
)
{
    auto const ip = config.get<std::string>("ip");
    auto const wsPort = config.get<std::string>("ws_port");
    auto const grpcPort = config.get<std::string>("grpc_port");

    etlng::impl::ForwardingSource forwardingSource{ip, wsPort, forwardingTimeout};
    impl::GrpcSource grpcSource{ip, grpcPort};
    auto subscriptionSource = std::make_unique<etl::impl::SubscriptionSource>(
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

}  // namespace etlng
