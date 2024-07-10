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

#include "rpc/Errors.hpp"
#include "util/log/Logger.hpp"
#include "util/requests/WsConnection.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace etl::impl {

class ForwardingSource {
    util::Logger log_;
    util::requests::WsConnectionBuilder connectionBuilder_;
    std::chrono::steady_clock::duration forwardingTimeout_;

    static constexpr std::chrono::seconds CONNECTION_TIMEOUT{3};

public:
    ForwardingSource(
        std::string ip,
        std::string wsPort,
        std::chrono::steady_clock::duration forwardingTimeout,
        std::chrono::steady_clock::duration connectionTimeout = CONNECTION_TIMEOUT
    );

    /**
     * @brief Forward a request to rippled.
     *
     * @param request The request to forward
     * @param forwardToRippledClientIp IP of the client forwarding this request if known
     * @param xUserValue Optional value for X-User header
     * @param yield The coroutine context
     * @return Response on success or error on failure
     */
    std::expected<boost::json::object, rpc::ClioError>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledClientIp,
        std::string_view xUserValue,
        boost::asio::yield_context yield
    ) const;
};

}  // namespace etl::impl
