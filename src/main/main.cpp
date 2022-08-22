#include <grpc/impl/codegen/port_platform.h>
#ifdef GRPC_TSAN_ENABLED
#undef GRPC_TSAN_ENABLED
#endif
#ifdef GRPC_ASAN_ENABLED
#undef GRPC_ASAN_ENABLED
#endif

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/json.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <algorithm>
#include <backend/BackendFactory.h>
#include <cstdlib>
#include <etl/ReportingETL.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <main/Build.h>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <webserver/Listener.h>

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
parse_certs(boost::json::object const& config)
{
    if (!config.contains("ssl_cert_file") || !config.contains("ssl_key_file"))
        return {};

    auto certFilename = config.at("ssl_cert_file").as_string().c_str();
    auto keyFilename = config.at("ssl_key_file").as_string().c_str();

    std::ifstream readCert(certFilename, std::ios::in | std::ios::binary);
    if (!readCert)
        return {};

    std::stringstream contents;
    contents << readCert.rdbuf();
    readCert.close();
    std::string cert = contents.str();

    std::ifstream readKey(keyFilename, std::ios::in | std::ios::binary);
    if (!readKey)
        return {};

    contents.str("");
    contents << readKey.rdbuf();
    readKey.close();
    std::string key = contents.str();

    ssl::context ctx{ssl::context::tlsv12};

    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2);

    ctx.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));

    ctx.use_private_key(
        boost::asio::buffer(key.data(), key.size()),
        boost::asio::ssl::context::file_format::pem);

    return ctx;
}

void
initLogging(boost::json::object const& config)
{
    namespace src = boost::log::sources;
    namespace keywords = boost::log::keywords;
    namespace sinks = boost::log::sinks;
    namespace trivial = boost::log::trivial;
    boost::log::add_common_attributes();
    std::string format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%";
    if (!config.contains("log_to_console") ||
        config.at("log_to_console").as_bool())
    {
        boost::log::add_console_log(std::cout, keywords::format = format);
    }
    if (config.contains("log_directory"))
    {
        if (!config.at("log_directory").is_string())
            throw std::runtime_error("log directory must be a string");
        boost::filesystem::path dirPath{
            config.at("log_directory").as_string().c_str()};
        if (!boost::filesystem::exists(dirPath))
            boost::filesystem::create_directories(dirPath);
        const int64_t rotationSize = config.contains("log_rotation_size")
            ? config.at("log_rotation_size").as_int64() * 1024 * 1024u
            : 2 * 1024 * 1024 * 1024u;
        if (rotationSize <= 0)
            throw std::runtime_error(
                "log rotation size must be greater than 0");
        const int64_t rotationPeriod =
            config.contains("log_rotation_hour_interval")
            ? config.at("log_rotation_hour_interval").as_int64()
            : 12u;
        if (rotationPeriod <= 0)
            throw std::runtime_error(
                "log rotation time interval must be greater than 0");
        const int64_t dirSize = config.contains("log_directory_max_size")
            ? config.at("log_directory_max_size").as_int64() * 1024 * 1024u
            : 50 * 1024 * 1024 * 1024u;
        if (dirSize <= 0)
            throw std::runtime_error(
                "log rotation directory max size must be greater than 0");
        auto fileSink = boost::log::add_file_log(
            keywords::file_name = dirPath / "clio.log",
            keywords::target_file_name = dirPath / "clio_%Y-%m-%d_%H-%M-%S.log",
            keywords::auto_flush = true,
            keywords::format = format,
            keywords::open_mode = std::ios_base::app,
            keywords::rotation_size = rotationSize,
            keywords::time_based_rotation =
                sinks::file::rotation_at_time_interval(
                    boost::posix_time::hours(rotationPeriod)));
        fileSink->locked_backend()->set_file_collector(
            sinks::file::make_collector(
                keywords::target = dirPath, keywords::max_size = dirSize));
        fileSink->locked_backend()->scan_for_files();
    }
    auto const logLevel = config.contains("log_level")
        ? config.at("log_level").as_string()
        : "info";
    if (boost::iequals(logLevel, "trace"))
        boost::log::core::get()->set_filter(
            trivial::severity >= trivial::trace);
    else if (boost::iequals(logLevel, "debug"))
        boost::log::core::get()->set_filter(
            trivial::severity >= trivial::debug);
    else if (boost::iequals(logLevel, "info"))
        boost::log::core::get()->set_filter(trivial::severity >= trivial::info);
    else if (
        boost::iequals(logLevel, "warning") || boost::iequals(logLevel, "warn"))
        boost::log::core::get()->set_filter(
            trivial::severity >= trivial::warning);
    else if (boost::iequals(logLevel, "error"))
        boost::log::core::get()->set_filter(
            trivial::severity >= trivial::error);
    else if (boost::iequals(logLevel, "fatal"))
        boost::log::core::get()->set_filter(
            trivial::severity >= trivial::fatal);
    else
    {
        BOOST_LOG_TRIVIAL(warning) << "Unrecognized log level: " << logLevel
                                   << ". Setting log level to info";
        boost::log::core::get()->set_filter(trivial::severity >= trivial::info);
    }
    BOOST_LOG_TRIVIAL(info) << "Log level = " << logLevel;
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
    if (argc != 2)
    {
        std::cerr << "Usage: clio_server "
                     "<config_file> \n"
                  << "Example:\n"
                  << "    clio_server config.json \n";
        return EXIT_FAILURE;
    }

    if (std::string{argv[1]} == "-v" || std::string{argv[1]} == "--version")
    {
        std::cout << Build::getClioFullVersionString() << std::endl;
        return EXIT_SUCCESS;
    }

    auto const config = parse_config(argv[1]);
    if (!config)
    {
        std::cerr << "Couldnt parse config. Exiting..." << std::endl;
        return EXIT_FAILURE;
    }

    initLogging(*config);

    // Announce Clio version
    BOOST_LOG_TRIVIAL(info)
        << "Clio version: " << Build::getClioFullVersionString();

    auto ctx = parse_certs(*config);
    auto ctxRef = ctx
        ? std::optional<std::reference_wrapper<ssl::context>>{ctx.value()}
        : std::nullopt;

    auto const threads = config->contains("io_threads")
        ? config->at("io_threads").as_int64()
        : 2;

    if (threads <= 0)
    {
        BOOST_LOG_TRIVIAL(fatal) << "io_threads is less than 0";
        return EXIT_FAILURE;
    }
    BOOST_LOG_TRIVIAL(info) << "Number of io threads = " << threads;

    // io context to handle all incoming requests, as well as other things
    // This is not the only io context in the application
    boost::asio::io_context ioc{threads};

    // Rate limiter, to prevent abuse
    DOSGuard dosGuard{config.value(), ioc};

    // Interface to the database
    std::shared_ptr<BackendInterface> backend{
        Backend::make_Backend(ioc, *config)};

    // Manages clients subscribed to streams
    std::shared_ptr<SubscriptionManager> subscriptions{
        SubscriptionManager::make_SubscriptionManager(*config, backend)};

    // Tracks which ledgers have been validated by the
    // network
    std::shared_ptr<NetworkValidatedLedgers> ledgers{
        NetworkValidatedLedgers::make_ValidatedLedgers()};

    // Handles the connection to one or more rippled nodes.
    // ETL uses the balancer to extract data.
    // The server uses the balancer to forward RPCs to a rippled node.
    // The balancer itself publishes to streams (transactions_proposed and
    // accounts_proposed)
    auto balancer = ETLLoadBalancer::make_ETLLoadBalancer(
        *config, ioc, ctxRef, backend, subscriptions, ledgers);

    // ETL is responsible for writing and publishing to streams. In read-only
    // mode, ETL only publishes
    auto etl = ReportingETL::make_ReportingETL(
        *config, ioc, backend, subscriptions, balancer, ledgers);

    // The server handles incoming RPCs
    auto httpServer = Server::make_HttpServer(
        *config, ioc, ctxRef, backend, subscriptions, balancer, etl, dosGuard);

    // Blocks until stopped.
    // When stopped, shared_ptrs fall out of scope
    // Calls destructors on all resources, and destructs in order
    start(ioc, threads);

    return EXIT_SUCCESS;
}
