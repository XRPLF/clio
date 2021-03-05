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
#include <reporting/ReportingETL.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

//------------------------------------------------------------------------------
enum RPCCommand {
    tx,
    account_tx,
    ledger,
    account_info,
    ledger_data,
    book_offers,
    ledger_range
};
std::unordered_map<std::string, RPCCommand> commandMap{
    {"tx", tx},
    {"account_tx", account_tx},
    {"ledger", ledger},
    {"ledger_range", ledger_range},
    {"account_info", account_info},
    {"ledger_data", ledger_data},
    {"book_offers", book_offers}};

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
            return doTx(request, backend);
            break;
        case account_tx:
            return doAccountTx(request, backend);
            break;
        case ledger:
            return doLedger(request, backend);
            break;
        case ledger_range:
            return doLedgerRange(request, backend);
            break;
        case ledger_data:
            return doLedgerData(request, backend);
            break;
        case account_info:
            return doAccountInfo(request, backend);
            break;
        case book_offers:
            return doBookOffers(request, backend);
            break;
        default:
            BOOST_LOG_TRIVIAL(error) << "Unknown command: " << command;
    }
    return response;
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

public:
    // Take ownership of the socket
    explicit session(
        boost::asio::ip::tcp::socket&& socket,
        BackendInterface const& backend)
        : ws_(std::move(socket)), backend_(backend)
    {
    }

    // Get on the correct executor
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
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
        boost::json::value raw = boost::json::parse(msg);
        // BOOST_LOG_TRIVIAL(debug) << __func__ << " parsed";
        boost::json::object request = raw.as_object();
        BOOST_LOG_TRIVIAL(debug) << " received request : " << request;
        auto response = buildResponse(request, backend_);
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

public:
    listener(
        boost::asio::io_context& ioc,
        boost::asio::ip::tcp::endpoint endpoint,
        BackendInterface const& backend)
        : ioc_(ioc), acceptor_(ioc), backend_(backend)
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
            std::make_shared<session>(std::move(socket), backend_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

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
    ReportingETL etl{config.value(), ioc};

    // Create and launch a listening port
    std::make_shared<listener>(
        ioc,
        boost::asio::ip::tcp::endpoint{address, port},
        etl.getFlatMapBackend())
        ->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });
    std::cout << "created ETL" << std::endl;
    etl.run();
    std::cout << "running ETL" << std::endl;
    ioc.run();

    return EXIT_SUCCESS;
}
