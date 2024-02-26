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
#include <cstdint>
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

    auto const cacheEntryTimeout = config.valueOr<uint64_t>("forwarding_cache_timeout_ms", 0);
    auto const cacheEntryTimeoutDuration = cacheEntryTimeout == 0
        ? std::nullopt
        : std::make_optional<std::chrono::steady_clock::duration>(std::chrono::milliseconds{cacheEntryTimeout});
    auto forwardingSource = std::make_unique<impl::ForwardingSource>(ip, wsPort, cacheEntryTimeoutDuration);

    impl::GrpcSource grpcSource{ip, grpcPort, std::move(backend)};

    auto onLedgerClosed = [&fs = *forwardingSource]() mutable { fs.invalidateCache(); };
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
