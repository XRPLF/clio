//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2021 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_REPORTING_HTTP_SESSION_H
#define RIPPLE_REPORTING_HTTP_SESSION_H

#include <reporting/server/HttpBase.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;       
using tcp = boost::asio::ip::tcp;

// Handles an HTTP server connection
class HttpSession : public HttpBase<HttpSession>
                  , public std::enable_shared_from_this<HttpSession>
{
    boost::beast::tcp_stream stream_;

public:
    // Take ownership of the socket
    explicit
    HttpSession(
        tcp::socket&& socket,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        boost::beast::flat_buffer buffer)
        : HttpBase<HttpSession>(backend, subscriptions, balancer, std::move(buffer))
        , stream_(std::move(socket))
    {}

    boost::beast::tcp_stream&
    stream()
    {
        return stream_;
    }

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this HttpSession. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(
            stream_.get_executor(),
            boost::beast::bind_front_handler(
                &HttpBase::do_read,
                shared_from_this()));
    }

    void
    do_close()
    {
        // Send a TCP shutdown
        boost::beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

#endif // RIPPLE_REPORTING_HTTP_SESSION_H