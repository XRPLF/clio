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
#include <reporting/server/Listener.h>
#include <reporting/server/WsSession.h>
#include <reporting/server/HttpSession.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

std::optional<ssl::context>
parse_certs(const char* certFilename, const char* keyFilename)
{
    std::ifstream readCert(certFilename, std::ios::in | std::ios::binary);
    if (!readCert)
        return {};

    std::stringstream contents;
    contents << readCert.rdbuf();
    readCert.close();
    std::string cert = contents.str();

    std::ifstream readKey(keyFilename, std::ios::in | std::ios::binary);
    if(!readKey)
        return {};

    contents.str("");
    contents << readKey.rdbuf();
    readKey.close();
    std::string key = contents.str();

    ssl::context ctx{ssl::context::tlsv12};

    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2);

    ctx.use_certificate_chain(
        boost::asio::buffer(cert.data(), cert.size()));

    ctx.use_private_key(
        boost::asio::buffer(key.data(), key.size()),
        boost::asio::ssl::context::file_format::pem);

    return ctx;
}


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

openWebsocketServer(
    boost::json::object const& wsConfig,
    boost::asio::io_context& ioc,
    ssl::context& ctx,
    ReportingETL& etl)
{
    auto const address = 
        boost::asio::ip::make_address(wsConfig.at("ip").as_string().c_str());
    auto const port = 
        static_cast<unsigned short>(wsConfig.at("port").as_int64());

    // Create and launch a listening port
    std::make_shared<Listener<WsSession>>(
        ioc,
        boost::asio::ip::tcp::endpoint{address, port},
        ctx,
        etl)
        ->run();
}

void
openHttpServer(
    boost::json::object const& httpConfig,
    boost::asio::io_context& ioc,
    std::optional<ssl::context>& ctx,
    ReportingETL& etl)
{
    auto const address = 
        boost::asio::ip::make_address(httpConfig.at("ip").as_string().c_str());
    auto const port = 
        static_cast<unsigned short>(httpConfig.at("port").as_int64());

    std::make_shared<Listener>(
        ioc,
        ctx,
        boost::asio::ip::tcp::endpoint{address, port},
        etl)
        ->run();
}

int
main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc < 3 || argc > 6)
    {
        std::cerr
            << "Usage: websocket-server-async <threads> "
               "<config_file> <cert_file> <key_file> <log level> \n"
            << "Example:\n"
            << "    websocket-server-async 1 config.json cert.pem key.pem 2\n";
        return EXIT_FAILURE;
    }

    auto const threads = std::max<int>(1, std::atoi(argv[1]));
    auto const config = parse_config(argv[2]);

    std::optional<ssl::context> ctx = {};
    if (argc == 4 || argc == 5)
        ctx = parse_certs(argv[3], argv[4]);

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

    boost::asio::io_context ioc{threads};
    
    std::shared_ptr<BackendInterface> backend{
        Backend::make_Backend(*config)
    };
    
    ReportingETL etl{config.value(), ioc};


    // if (config->contains("websocket_public"))
    // {
    //     BOOST_LOG_TRIVIAL(info) << "Starting Websocket Server";

    //     auto wsConfig = config->at("websocket_public");
    //     if(!wsConfig.is_object())
    //         throw std::runtime_error("Websocket config must be JSON object");

    //     openWebsocketServer(wsConfig.as_object(), ioc, *ctx, etl);
    // }


    std::shared_ptr<SubscriptionManager> subscriptions{
        SubscriptionManager::make_SubscriptionManager()
    };

    std::shared_ptr<NetworkValidatedLedgers> ledgers{
        NetworkValidatedLedgers::make_ValidatedLedgers()
    };

    std::shared_ptr<ETLLoadBalancer> balancer{ETLLoadBalancer::make_ETLLoadBalancer(
        *config,
        ioc,
        backend,
        subscriptions,
        ledgers
    )};

    std::shared_ptr<ReportingETL> etl{ReportingETL::make_ReportingETL(
        *config,
        ioc,
        backend,
        subscriptions,
        balancer,
        ledgers
    )};

    listener::make_listener(
        ioc,
        boost::asio::ip::tcp::endpoint{address, port},
        backend,
        subscriptions,
        balancer
    );

    // Blocks until stopped.
    // When stopped, shared_ptrs fall out of scope
    // Calls destructors on all resources, and destructs in order
    start(ioc, threads);
    std::cout << "Out Of Scope" << std::endl;

    return EXIT_SUCCESS;
}
