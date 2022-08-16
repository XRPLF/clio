#ifndef LISTENER_H
#define LISTENER_H

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <subscriptions/SubscriptionManager.h>
#include <webserver/HttpSession.h>
#include <webserver/PlainWsSession.h>
#include <webserver/SslHttpSession.h>
#include <webserver/SslWsSession.h>

#include <iostream>

class SubscriptionManager;

template <class PlainSession, class SslSession>
class Detector
    : public std::enable_shared_from_this<Detector<PlainSession, SslSession>>
{
    using std::enable_shared_from_this<
        Detector<PlainSession, SslSession>>::shared_from_this;

    Application const& app_;
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;

public:
    Detector(Application const& app, boost::asio::ip::tcp::socket&& socket)
        : app_(app), stream_(std::move(socket))
    {
    }

    inline void
    fail(boost::system::error_code ec, char const* message)
    {
        if (ec == net::ssl::error::stream_truncated)
            return;

        BOOST_LOG_TRIVIAL(info)
            << "Detector failed: " << message << ec.message() << std::endl;
    }

    // Launch the detector
    void
    run()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(
            std::chrono::seconds(30));
        // Detect a TLS handshake
        async_detect_ssl(
            stream_,
            buffer_,
            boost::beast::bind_front_handler(
                &Detector::on_detect, shared_from_this()));
    }

    void
    on_detect(boost::beast::error_code ec, bool result)
    {
        if (ec)
            return fail(ec, "detect");

        if (result)
        {
            if (!app_.sslContext())
                return fail(ec, "ssl not supported by this server");
            // Launch SSL session
            std::make_shared<SslSession>(
                app_, stream_.release_socket(), std::move(buffer_))
                ->run();
            return;
        }

        // Launch plain session
        std::make_shared<PlainSession>(
            app_, stream_.release_socket(), std::move(buffer_))
            ->run();
    }
};

static void
make_websocket_session(
    Application const& app,
    boost::beast::tcp_stream stream,
    http::request<http::string_body> req,
    boost::beast::flat_buffer buffer)
{
    std::make_shared<WsUpgrader>(
        app, std::move(stream), std::move(buffer), std::move(req))
        ->run();
}

static void
make_websocket_session(
    Application const& app,
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
    http::request<http::string_body> req,
    boost::beast::flat_buffer buffer)
{
    std::make_shared<SslWsUpgrader>(
        app, std::move(stream), std::move(buffer), std::move(req))
        ->run();
}

template <class PlainSession, class SslSession>
class Listener
    : public std::enable_shared_from_this<Listener<PlainSession, SslSession>>
{
    using std::enable_shared_from_this<
        Listener<PlainSession, SslSession>>::shared_from_this;

    Application const& app_;
    tcp::acceptor acceptor_;

public:
    Listener(Application const& app)
        : app_(app), acceptor_(net::make_strand(app.socketIoc()))
    {
        boost::beast::error_code ec;

        ServerConfig const& config = *app.config().server;
        auto const address = boost::asio::ip::make_address(config.ip);
        auto const port = static_cast<unsigned short>(config.port);

        auto endpoint = boost::asio::ip::tcp::endpoint{address, port};

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            return;

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
            return;

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
            return;

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
            return;
    }

    // Start accepting incoming connections
    void
    run()
    {
        do_accept();
    }

private:
    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(app_.socketIoc()),
            boost::beast::bind_front_handler(
                &Listener::on_accept, shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec, tcp::socket socket)
    {
        if (!ec)
        {
            std::make_shared<Detector<PlainSession, SslSession>>(
                app_, std::move(socket))
                ->run();
        }

        // Accept another connection
        do_accept();
    }
};

namespace Server {

std::shared_ptr<HttpServer>
make_HttpServer(Application const& app);

}  // namespace Server

#endif  // LISTENER_H
