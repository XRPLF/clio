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

#include "etl/impl/SubscribedSource.h"

#include "etl/ETLHelpers.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http/field.hpp>
#include <fmt/core.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace etl::impl {

SubscribedSource::SubscribedSource(
    boost::asio::io_context& ioContext,
    std::string const& ip,
    std::string const& wsPort,
    std::shared_ptr<NetworkValidatedLedgers> validatedLedgers
)
    : log_(fmt::format("GrpcSource-{}:{}", ip, wsPort))
    , wsConnectionBuilder_(ip, wsPort)
    , networkValidatedLedgers_(std::move(validatedLedgers))
    , strand_(boost::asio::make_strand(ioContext))
{
    wsConnectionBuilder_.addHeader({boost::beast::http::field::user_agent, "clio-client"})
        .addHeader({"X-User", "clio-client"});
    subscribe();
}

bool
SubscribedSource::hasLedger(uint32_t sequence) const
{
    auto validatedLedgers = validatedLedgers_.lock();
    for (auto& pair : validatedLedgers.get()) {
        if (sequence >= pair.first && sequence <= pair.second) {
            return true;
        }
        if (sequence < pair.first) {
            // validatedLedgers_ is a sorted list of disjoint ranges
            // if the sequence comes before this range, the sequence will
            // come before all subsequent ranges
            return false;
        }
    }
    return false;
}

void
SubscribedSource::subscribe()
{
    // boost::asio::post(strand_, [this](boost::asio::yield_context yield) {
    //     bool connected = false;
    //     while (not connected) {
    //         auto connection = wsConnectionBuilder_.connect(yield);
    //         if (not connection.has_value()) {
    //         }
    //     }
    // });
}

}  // namespace etl::impl
