#pragma once

#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>

#include <functional>

namespace web::ng {

/**
 * @brief Handler for messages.
 */
using MessageHandler = std::function<Response(
    Request const&,
    ConnectionMetadata&,
    SubscriptionContextPtr,
    boost::asio::yield_context
)>;

}  // namespace web::ng
