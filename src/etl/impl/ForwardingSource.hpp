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

    static constexpr std::chrono::seconds kCONNECTION_TIMEOUT{3};

public:
    ForwardingSource(
        std::string ip,
        std::string wsPort,
        std::chrono::steady_clock::duration forwardingTimeout,
        std::chrono::steady_clock::duration connTimeout = ForwardingSource::kCONNECTION_TIMEOUT
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
    [[nodiscard]] std::expected<boost::json::object, rpc::ClioError>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledClientIp,
        std::string_view xUserValue,
        boost::asio::yield_context yield
    ) const;
};

}  // namespace etl::impl
