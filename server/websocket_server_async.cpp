//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: WebSocket server, asynchronous
//
//------------------------------------------------------------------------------

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <reporting/BackendFactory.h>
#include <reporting/ReportingETL.h>
#include <reporting/server/listener.h>
#include <reporting/server/session.h>
#include <server/DOSGuard.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

<<<<<<< HEAD:websocket_server_async.cpp
=======
//------------------------------------------------------------------------------
enum RPCCommand {
    tx,
    account_tx,
    ledger,
    account_info,
    ledger_data,
    book_offers,
    ledger_range,
    ledger_entry,
    server_info
};
std::unordered_map<std::string, RPCCommand> commandMap{
    {"tx", tx},
    {"account_tx", account_tx},
    {"ledger", ledger},
    {"ledger_range", ledger_range},
    {"ledger_entry", ledger_entry},
    {"account_info", account_info},
    {"ledger_data", ledger_data},
    {"book_offers", book_offers},
    {"server_info", server_info}};

boost::json::object
doAccountInfo(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doTx(boost::json::object const& request, BackendInterface const& backend);
boost::json::object
doAccountTx(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedgerData(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedgerEntry(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doBookOffers(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedger(boost::json::object const& request, BackendInterface const& backend);
boost::json::object
doLedgerRange(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doServerInfo(
    boost::json::object const& request,
    BackendInterface const& backend);

std::pair<boost::json::object, uint32_t>
buildResponse(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    std::string command = request.at("command").as_string().c_str();
    BOOST_LOG_TRIVIAL(info) << "Received rpc command : " << request;
    boost::json::object response;
    switch (commandMap[command])
    {
        case tx:
            return {doTx(request, backend), 1};
            break;
        case account_tx: {
            auto res = doAccountTx(request, backend);
            if (res.contains("transactions"))
                return {res, res["transactions"].as_array().size()};
            return {res, 1};
        }
        break;
        case ledger: {
            auto res = doLedger(request, backend);
            if (res.contains("transactions"))
                return {res, res["transactions"].as_array().size()};
            return {res, 1};
        }
        break;
        case ledger_entry:
            return {doLedgerEntry(request, backend), 1};
            break;
        case ledger_range:
            return {doLedgerRange(request, backend), 1};
            break;
        case ledger_data: {
            auto res = doLedgerData(request, backend);
            if (res.contains("objects"))
                return {res, res["objects"].as_array().size()};
            return {res, 1};
        }
        break;
        case server_info: {
            return {doServerInfo(request, backend), 1};
            break;
        }
        case account_info:
            return {doAccountInfo(request, backend), 1};
            break;
        case book_offers: {
            auto res = doBookOffers(request, backend);
            if (res.contains("offers"))
                return {res, res["offers"].as_array().size()};
            return {res, 1};
        }
        break;
        default:
            BOOST_LOG_TRIVIAL(error) << "Unknown command: " << command;
    }
    return {response, 1};
}
// Report a failure
void
fail(boost::beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class session : public std::enable_shared_from_this<session>
{
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    boost::beast::flat_buffer buffer_;
    std::string response_;
    BackendInterface const& backend_;
    DOSGuard& dosGuard_;

public:
    // Take ownership of the socket
    explicit session(
        boost::asio::ip::tcp::socket&& socket,
        BackendInterface const& backend,
        DOSGuard& dosGuard)
        : ws_(std::move(socket)), backend_(backend), dosGuard_(dosGuard)
    {
    }

    // Get on the correct executor
    void
    run()
    {
        // We need to be executing within a strand to perform async
        // operations on the I/O objects in this session. Although not
        // strictly necessary for single-threaded contexts, this example
        // code is written to be thread-safe by default.
        boost::asio::dispatch(
            ws_.get_executor(),
            boost::beast::bind_front_handler(
                &session::on_run, shared_from_this()));
    }

    // Start the asynchronous operation
    void
    on_run()
    {
        // Set suggested timeout settings for the websocket
        ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::response_type& res) {
                res.set(
                    boost::beast::http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async");
            }));
        // Accept the websocket handshake
        ws_.async_accept(boost::beast::bind_front_handler(
            &session::on_accept, shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec)
    {
        if (ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            boost::beast::bind_front_handler(
                &session::on_read, shared_from_this()));
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == boost::beast::websocket::error::closed)
            return;

        if (ec)
            fail(ec, "read");
        std::string msg{
            static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        // BOOST_LOG_TRIVIAL(debug) << __func__ << msg;
        boost::json::object response;
        auto ip =
            ws_.next_layer().socket().remote_endpoint().address().to_string();
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " received request from ip = " << ip;
        if (!dosGuard_.isOk(ip))
            response["error"] = "Too many requests. Slow down";
        else
        {
            try
            {
                boost::json::value raw = boost::json::parse(msg);
                boost::json::object request = raw.as_object();
                BOOST_LOG_TRIVIAL(debug) << " received request : " << request;
                try
                {
                    auto start = std::chrono::system_clock::now();
                    auto [res, cost] = buildResponse(request, backend_);
                    response = std::move(res);
                    if (!dosGuard_.add(ip, cost))
                    {
                        response["warning"] = "Too many requests";
                    }

                    auto end = std::chrono::system_clock::now();
                    BOOST_LOG_TRIVIAL(info)
                        << __func__ << " RPC call took "
                        << ((end - start).count() / 1000000000.0)
                        << " . request = " << request;
                }
                catch (Backend::DatabaseTimeout const& t)
                {
                    BOOST_LOG_TRIVIAL(error) << __func__ << " Database timeout";
                    response["error"] =
                        "Database read timeout. Please retry the request";
                }
            }
            catch (std::exception const& e)
            {
                BOOST_LOG_TRIVIAL(error)
                    << __func__ << "caught exception : " << e.what();
                response["error"] = "Unknown exception";
            }
        }
        BOOST_LOG_TRIVIAL(trace) << __func__ << response;
        response_ = boost::json::serialize(response);

        // Echo the message
        ws_.text(ws_.got_text());
        ws_.async_write(
            boost::asio::buffer(response_),
            boost::beast::bind_front_handler(
                &session::on_write, shared_from_this()));
    }

    void
    on_write(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    BackendInterface const& backend_;
    DOSGuard& dosGuard_;

public:
    listener(
        boost::asio::io_context& ioc,
        boost::asio::ip::tcp::endpoint endpoint,
        BackendInterface const& backend,
        DOSGuard& dosGuard)
        : ioc_(ioc), acceptor_(ioc), backend_(backend), dosGuard_(dosGuard)
    {
        boost::beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
        {
            fail(ec, "listen");
            return;
        }
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
            boost::asio::make_strand(ioc_),
            boost::beast::bind_front_handler(
                &listener::on_accept, shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the session and run it
            std::make_shared<session>(std::move(socket), backend_, dosGuard_)
                ->run();
        }

        // Accept another connection
        do_accept();
    }
};

>>>>>>> dev:server/websocket_server_async.cpp
std::optional<boost::json::object>
parse_config(const char* filename)
{
    try
    {
        std::ifstream in(filename, std::ios::in | std::ios::binary);
        if (in)
        {
            std::stringstream contents;
            contents << in.rdbuf();
            in.close();
            std::cout << contents.str() << std::endl;
            boost::json::value value = boost::json::parse(contents.str());
            return value.as_object();
        }
    }
    catch (std::exception const& e)
    {
        std::cout << e.what() << std::endl;
    }
    return {};
}
//------------------------------------------------------------------------------
//
void
initLogLevel(int level)
{
    switch (level)
    {
        case 0:
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::trace);
            break;
        case 1:
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::debug);
            break;
        case 2:
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::info);
            break;
        case 3:
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);
            break;
        case 4:
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::error);
            break;
        case 5:
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::fatal);
            break;
        default:
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::info);
    }
}

void
start(boost::asio::io_context& ioc, std::uint32_t numThreads)
{
    std::vector<std::thread> v;
    v.reserve(numThreads - 1);
    for (auto i = numThreads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });

    ioc.run();
}

int
main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 5 and argc != 6)
    {
        std::cerr
            << "Usage: websocket-server-async <address> <port> <threads> "
               "<config_file> <log level> \n"
            << "Example:\n"
            << "    websocket-server-async 0.0.0.0 8080 1 config.json 2\n";
        return EXIT_FAILURE;
    }
    auto const address = boost::asio::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));
    auto const config = parse_config(argv[4]);
    if (argc > 5)
    {
        initLogLevel(std::atoi(argv[5]));
    }
    else
    {
        initLogLevel(2);
    }
    if (!config)
    {
        std::cerr << "couldnt parse config. Exiting..." << std::endl;
        return EXIT_FAILURE;
    }

    // The io_context is required for all I/O
    boost::asio::io_context ioc{threads};
<<<<<<< HEAD:websocket_server_async.cpp

    std::shared_ptr<BackendInterface> backend{Backend::make_Backend(*config)};

    std::shared_ptr<SubscriptionManager> subscriptions{
        SubscriptionManager::make_SubscriptionManager()};
=======
    ReportingETL etl{config.value(), ioc};
    DOSGuard dosGuard{config.value(), ioc};
>>>>>>> dev:server/websocket_server_async.cpp

    std::shared_ptr<NetworkValidatedLedgers> ledgers{
        NetworkValidatedLedgers::make_ValidatedLedgers()};

    std::shared_ptr<ETLLoadBalancer> balancer{
        ETLLoadBalancer::make_ETLLoadBalancer(
            *config,
            ioc,
<<<<<<< HEAD:websocket_server_async.cpp
            backend,
            subscriptions,
            ledgers)};
=======
            boost::asio::ip::tcp::endpoint{address, port},
            etl.getFlatMapBackend(),
            dosGuard)
            ->run();
>>>>>>> dev:server/websocket_server_async.cpp

    std::shared_ptr<ReportingETL> etl{ReportingETL::make_ReportingETL(
        *config, ioc, backend, subscriptions, balancer, ledgers)};

    listener::make_listener(
        ioc,
        boost::asio::ip::tcp::endpoint{address, port},
        backend,
        subscriptions,
        balancer);

    // Blocks until stopped.
    // When stopped, shared_ptrs fall out of scope
    // Calls destructors on all resources, and destructs in order
    start(ioc, threads);

    return EXIT_SUCCESS;
}
