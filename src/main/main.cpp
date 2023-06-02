//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022-2023, the clio developers.

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

#include <grpc/impl/codegen/port_platform.h>
#ifdef GRPC_TSAN_ENABLED
#undef GRPC_TSAN_ENABLED
#endif
#ifdef GRPC_ASAN_ENABLED
#undef GRPC_ASAN_ENABLED
#endif

#include <backend/BackendFactory.h>
#include <config/Config.h>
#include <etl/ETLService.h>
#include <log/Logger.h>
#include <rpc/Counters.h>
#include <rpc/RPCEngine.h>
#include <rpc/common/impl/HandlerProvider.h>
#include <webserver/Listener.h>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <main/Build.h>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace clio;
namespace po = boost::program_options;

/**
 * @brief Parse command line and return path to configuration file
 *
 * @param argc
 * @param argv
 * @return std::string Path to configuration file
 */
std::string
parseCli(int argc, char* argv[])
{
    static constexpr char defaultConfigPath[] = "/etc/opt/clio/config.json";

    // clang-format off
    po::options_description description("Options");
    description.add_options()
        ("help,h", "print help message and exit")
        ("version,v", "print version and exit")
        ("conf,c", po::value<std::string>()->default_value(defaultConfigPath), "configuration file")
    ;
    // clang-format on
    po::positional_options_description positional;
    positional.add("conf", 1);

    po::variables_map parsed;
    po::store(po::command_line_parser(argc, argv).options(description).positional(positional).run(), parsed);
    po::notify(parsed);

    if (parsed.count("version"))
    {
        std::cout << Build::getClioFullVersionString() << '\n';
        std::exit(EXIT_SUCCESS);
    }

    if (parsed.count("help"))
    {
        std::cout << "Clio server " << Build::getClioFullVersionString() << "\n\n" << description;
        std::exit(EXIT_SUCCESS);
    }

    return parsed["conf"].as<std::string>();
}

/**
 * @brief Parse certificates from configuration file
 *
 * @param config The configuration
 * @return std::optional<ssl::context> SSL context if certificates were parsed
 */
std::optional<ssl::context>
parseCerts(Config const& config)
{
    if (!config.contains("ssl_cert_file") || !config.contains("ssl_key_file"))
        return {};

    auto certFilename = config.value<std::string>("ssl_cert_file");
    auto keyFilename = config.value<std::string>("ssl_key_file");

    std::ifstream readCert(certFilename, std::ios::in | std::ios::binary);
    if (!readCert)
        return {};

    std::stringstream contents;
    contents << readCert.rdbuf();
    std::string cert = contents.str();

    std::ifstream readKey(keyFilename, std::ios::in | std::ios::binary);
    if (!readKey)
        return {};

    contents.str("");
    contents << readKey.rdbuf();
    readKey.close();
    std::string key = contents.str();

    ssl::context ctx{ssl::context::tlsv12};
    ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2);
    ctx.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));
    ctx.use_private_key(boost::asio::buffer(key.data(), key.size()), boost::asio::ssl::context::file_format::pem);

    return ctx;
}

/**
 * @brief Start context threads
 *
 * @param ioc Context
 * @param numThreads Number of worker threads to start
 */
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
try
{
    auto const configPath = parseCli(argc, argv);
    auto const config = ConfigReader::open(configPath);
    if (!config)
    {
        std::cerr << "Couldnt parse config '" << configPath << "'." << std::endl;
        return EXIT_FAILURE;
    }

    LogService::init(config);
    LogService::info() << "Clio version: " << Build::getClioFullVersionString();

    auto ctx = parseCerts(config);
    auto ctxRef = ctx ? std::optional<std::reference_wrapper<ssl::context>>{ctx.value()} : std::nullopt;

    auto const threads = config.valueOr("io_threads", 2);
    if (threads <= 0)
    {
        LogService::fatal() << "io_threads is less than 0";
        return EXIT_FAILURE;
    }
    LogService::info() << "Number of io threads = " << threads;

    // IO context to handle all incoming requests, as well as other things.
    // This is not the only io context in the application.
    boost::asio::io_context ioc{threads};

    // Rate limiter, to prevent abuse
    auto sweepHandler = IntervalSweepHandler{config, ioc};
    auto dosGuard = DOSGuard{config, sweepHandler};

    // Interface to the database
    auto backend = Backend::make_Backend(ioc, config);

    // Manages clients subscribed to streams
    auto subscriptions = SubscriptionManager::make_SubscriptionManager(config, backend);

    // Tracks which ledgers have been validated by the network
    auto ledgers = NetworkValidatedLedgers::make_ValidatedLedgers();

    // Handles the connection to one or more rippled nodes.
    // ETL uses the balancer to extract data.
    // The server uses the balancer to forward RPCs to a rippled node.
    // The balancer itself publishes to streams (transactions_proposed and accounts_proposed)
    auto balancer = LoadBalancer::make_LoadBalancer(config, ioc, backend, subscriptions, ledgers);

    // ETL is responsible for writing and publishing to streams. In read-only mode, ETL only publishes
    auto etl = ETLService::make_ETLService(config, ioc, backend, subscriptions, balancer, ledgers);

    auto workQueue = WorkQueue::make_WorkQueue(config);
    auto counters = RPC::Counters::make_Counters(workQueue);
    auto const handlerProvider =
        std::make_shared<RPC::detail::ProductionHandlerProvider const>(backend, subscriptions, balancer, etl, counters);
    auto const rpcEngine = RPC::RPCEngine::make_RPCEngine(
        config, backend, subscriptions, balancer, etl, dosGuard, workQueue, counters, handlerProvider);

    // The server handles incoming RPCs
    auto httpServer =
        Server::make_HttpServer(config, ioc, ctxRef, backend, rpcEngine, subscriptions, balancer, etl, dosGuard);

    // Blocks until stopped.
    // When stopped, shared_ptrs fall out of scope
    // Calls destructors on all resources, and destructs in order
    start(ioc, threads);

    return EXIT_SUCCESS;
}
catch (std::exception const& e)
{
    LogService::fatal() << "Exit on exception: " << e.what();
}
