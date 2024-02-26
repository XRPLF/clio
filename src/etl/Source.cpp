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
#include "feed/SubscriptionManager.hpp"
#include "util/config/Config.hpp"

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace etl {

template class SourceImpl<>;

Source
make_Source(
    util::Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> validatedLedgers,
    Source::OnDisconnectHook onDisconnect,
    Source::OnConnectHook onConnect
)
{
    auto const ip = config.valueOr<std::string>("ip", {});
    auto const wsPort = config.valueOr<std::string>("ws_port", {});
    auto const grpcPort = config.valueOr<std::string>("grpc_port", {});

    auto const cacheEntryTimeout = config.valueOr<float>("forwarding_cache_timeout", 0.f);
    std::optional<std::chrono::steady_clock::duration> cacheEntryTimeoutDuration;
    if (cacheEntryTimeout != 0.f) {
        cacheEntryTimeoutDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<float>{cacheEntryTimeout});
    }
    auto forwardingSource = std::make_shared<impl::ForwardingSource>(ip, wsPort, cacheEntryTimeoutDuration);

    impl::GrpcSource grpcSource{ip, grpcPort, std::move(backend)};

    auto onLedgerClosed = [fs = forwardingSource]() mutable { fs->invalidateCache(); };
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
