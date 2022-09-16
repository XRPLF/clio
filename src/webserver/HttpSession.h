#ifndef RIPPLE_REPORTING_HTTP_SESSION_H
#define RIPPLE_REPORTING_HTTP_SESSION_H

#include <webserver/HttpBase.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// Handles an HTTP server connection
class HttpSession : public HttpBase<HttpSession>,
                    public std::enable_shared_from_this<HttpSession>
{
    boost::beast::tcp_stream stream_;

public:
    // Take ownership of the socket
    explicit HttpSession(
        boost::asio::io_context& ioc,
        tcp::socket&& socket,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<ReportingETL const> etl,
        util::TagDecoratorFactory const& tagFactory,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        WorkQueue& queue,
        boost::beast::flat_buffer buffer)
        : HttpBase<HttpSession>(
              ioc,
              backend,
              subscriptions,
              balancer,
              etl,
              tagFactory,
              dosGuard,
              counters,
              queue,
              std::move(buffer))
        , stream_(std::move(socket))
    {
    }

    boost::beast::tcp_stream&
    stream()
    {
        return stream_;
    }
    boost::beast::tcp_stream
    release_stream()
    {
        return std::move(stream_);
    }

    std::optional<std::string>
    ip()
    {
        try
        {
            return stream_.socket().remote_endpoint().address().to_string();
        }
        catch (std::exception const&)
        {
            return {};
        }
    }

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this HttpSession. Although not strictly
        // necessary for single-threaded contexts, this example code is written
        // to be thread-safe by default.
        net::dispatch(
            stream_.get_executor(),
            boost::beast::bind_front_handler(
                &HttpBase::do_read, shared_from_this()));
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

#endif  // RIPPLE_REPORTING_HTTP_SESSION_H
