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
#include "etl/ETLHelpers.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/config/Config.hpp"

#include <boost/asio/io_context.hpp>

#include <memory>
#include <string>
#include <utility>

namespace etl {

template class SourceImpl<>;

Source
make_Source(
    util::Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    Source::OnDisconnectHook onDisconnect,
    Source::OnConnectHook onConnect,
    Source::OnLedgerClosedHook onLedgerClosed
)
{
    auto const ip = config.valueOr<std::string>("ip", {});
    auto const wsPort = config.valueOr<std::string>("ws_port", {});
    auto const grpcPort = config.valueOr<std::string>("grpc_port", {});

    impl::ForwardingSource forwardingSource{ip, wsPort};
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

    return Source{
        ip, wsPort, grpcPort, std::move(grpcSource), std::move(subscriptionSource), std::move(forwardingSource)
    };
}

}  // namespace etl
