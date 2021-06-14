//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

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

#ifndef LISTENER_H
#define LISTENER_H

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <server/SubscriptionManager.h>

#include <iostream>

class SubscriptionManager;

// Accepts incoming connections and launches the sessions
template <class Session>
class listener : public std::enable_shared_from_this<listener<Session>>
{
    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
<<<<<<< HEAD:server/listener.h
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;
=======
    ReportingETL& etl_;
>>>>>>> 27506bc (rebase handlers):reporting/server/listener.h

public:
    static void
    make_listener(
        boost::asio::io_context& ioc,
        boost::asio::ip::tcp::endpoint endpoint,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard)
    {
        std::make_shared<listener>(
            ioc, endpoint, backend, subscriptions, balancer, dosGuard)
            ->run();
    }

    listener(
        boost::asio::io_context& ioc,
        boost::asio::ip::tcp::endpoint endpoint,
<<<<<<< HEAD:server/listener.h
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard)
        : ioc_(ioc)
        , acceptor_(ioc)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
=======
        ReportingETL& etl)
        : ioc_(ioc)
        , acceptor_(ioc)
        , etl_(etl)
>>>>>>> 27506bc (rebase handlers):reporting/server/listener.h
    {
        boost::beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "Could not open acceptor: "
                                     << ec.message();
            return;
        }

        // Allow address reuse
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "Could not set option for acceptor: "
                                     << ec.message();
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "Could not bind acceptor: "
                                     << ec.message();
            return;
        }

        // Start listening for connections
        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "Acceptor could not start listening: "
                                     << ec.message();
            return;
        }
    }

    ~listener() = default;

private:
    void
    run()
    {
        do_accept();
    }

    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            boost::asio::make_strand(ioc_),
            boost::beast::bind_front_handler(
                &listener::on_accept, shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
    {
        if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "Failed to accept: "
                                     << ec.message();
        }
        else
        {
<<<<<<< HEAD:server/listener.h
            session::make_session(
                std::move(socket),
                backend_,
                subscriptions_,
                balancer_,
                dosGuard_);
=======
            // Create the session and run it
            std::make_shared<Session>(std::move(socket), etl_)->run();
>>>>>>> 27506bc (rebase handlers):reporting/server/listener.h
        }

        // Accept another connection
        do_accept();
    }
};

#endif  // LISTENER_H
