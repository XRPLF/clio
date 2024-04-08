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

#pragma once

#include "util/log/Logger.hpp"
#include "util/requests/WsConnection.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <optional>
#include <string>

namespace etl::impl {

class ForwardingSource {
    util::Logger log_;
    util::requests::WsConnectionBuilder connectionBuilder_;

    static constexpr std::chrono::seconds CONNECTION_TIMEOUT{3};

public:
    ForwardingSource(
        std::string ip_,
        std::string wsPort_,
        std::chrono::steady_clock::duration connectionTimeout = CONNECTION_TIMEOUT
    );

    /**
     * @brief Forward a request to rippled.
     *
     * @param request The request to forward
     * @param forwardToRippledClientIp IP of the client forwarding this request if known
     * @param yield The coroutine context
     * @return Response wrapped in an optional on success; nullopt otherwise
     */
    static std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledClientIp,
        boost::asio::yield_context yield
    );
};

}  // namespace etl::impl
