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
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <backend/BackendFactory.h>
#include <etl/ReportingETL.h>
#include <main/Application.h>
#include <rpc/WorkQueue.h>
#include <webserver/HttpSession.h>
#include <webserver/Listener.h>
#include <webserver/SslHttpSession.h>

std::optional<ssl::context>
ApplicationImp::parseCerts(Config const& config)
{
    if (!config.sslCertFile || !config.sslKeyFile)
        return {};

    auto certFilename = *config.sslCertFile;
    auto keyFilename = *config.sslKeyFile;

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
ApplicationImp::initLogging(Config const& config)
{
    namespace src = boost::log::sources;
    namespace keywords = boost::log::keywords;
    namespace sinks = boost::log::sinks;
    namespace trivial = boost::log::trivial;

    boost::log::add_common_attributes();
    std::string format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%";

    if (config.logToConsole)
    {
        boost::log::add_console_log(std::cout, keywords::format = format);
    }

    if (config.logDirectory)
    {
        boost::filesystem::path dirPath{*config.logDirectory};

        if (!boost::filesystem::exists(dirPath))
            boost::filesystem::create_directories(dirPath);

        const int64_t rotationSize = config.logRotationSize;

        if (rotationSize <= 0)
            throw std::runtime_error(
                "log rotation size must be greater than 0");

        const int64_t rotationPeriod = config.logRotationHourInterval;

        if (rotationPeriod <= 0)
            throw std::runtime_error(
                "log rotation time interval must be greater than 0");

        const int64_t dirSize = config.logDirectoryMaxSize;

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

    auto const logLevel = config.logLevel;

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

ApplicationImp::ApplicationImp(std::unique_ptr<Config>&& config)
    : rpcContext_()
    , etlContext_()
    , config_(std::move(config))
    , sslContext_(parseCerts(*config_))
    , counters_(std::make_unique<RPC::Counters>())
    , queue_(std::make_unique<RPC::WorkQueue>(*this))
    , dosGuard_(std::make_unique<DOSGuard>(*this))
    , backend_(Backend::make_Backend(*this))
    , subscriptions_(SubscriptionManager::make_SubscriptionManager(*this))
    , ledgers_(NetworkValidatedLedgers::make_ValidatedLedgers())
    , balancer_(ETLLoadBalancer::make_ETLLoadBalancer(*this))
    , etl_(ReportingETL::make_ReportingETL(*this))
    , httpServer_(Server::make_HttpServer(*this))
{
}

ApplicationImp::~ApplicationImp() = default;

std::unique_ptr<Application>
make_Application(std::unique_ptr<Config> config)
{
    return std::make_unique<ApplicationImp>(std::move(config));
}