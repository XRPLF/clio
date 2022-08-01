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

#include <clio/backend/BackendFactory.h>
#include <clio/etl/ReportingETL.h>
#include <clio/main/Application.h>
#include <clio/rpc/WorkQueue.h>
#include <clio/webserver/HttpSession.h>
#include <clio/webserver/Listener.h>
#include <clio/webserver/SslHttpSession.h>

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
    boost::log::add_common_attributes();
    std::string format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%";
    boost::log::add_console_log(
        std::cout, boost::log::keywords::format = format);
    if (config.logFile)
    {
        boost::log::add_file_log(
            *config.logFile,
            boost::log::keywords::format = format,
            boost::log::keywords::open_mode = std::ios_base::app);
    }
    auto const logLevel = config.logLevel;
    if (boost::iequals(logLevel, "trace"))
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::trace);
    else if (boost::iequals(logLevel, "debug"))
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::debug);
    else if (boost::iequals(logLevel, "info"))
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::info);
    else if (
        boost::iequals(logLevel, "warning") || boost::iequals(logLevel, "warn"))
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::warning);
    else if (boost::iequals(logLevel, "error"))
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::error);
    else if (boost::iequals(logLevel, "fatal"))
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::fatal);
    else
    {
        BOOST_LOG_TRIVIAL(warning) << "Unrecognized log level: " << logLevel
                                   << ". Setting log level to info";
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::info);
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