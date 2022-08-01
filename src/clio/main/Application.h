#ifndef CLIO_APPLICATION_H
#define CLIO_APPLICATION_H

#include <memory>
#include <optional>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <clio/main/Config.h>

namespace RPC {
class Counters;
class WorkQueue;
}  // namespace RPC

namespace Backend {
class BackendInterface;
}

class DOSGuard;
class NetworkValidatedLedgers;
class ReportingETL;
class ETLLoadBalancer;
class SubscriptionManager;

class HttpSession;
class SslHttpSession;

template <class PlainSession, class SslSession>
class Listener;

namespace Server {
using HttpServer = Listener<HttpSession, SslHttpSession>;
}

class Application
{
public:
    virtual ~Application(){};

    virtual Config const&
    config() const = 0;

    virtual boost::asio::io_context&
    rpc() const = 0;

    virtual boost::asio::io_context&
    etl() const = 0;

    virtual RPC::Counters&
    counters() const = 0;

    virtual std::optional<boost::asio::ssl::context>&
    sslContext() const = 0;

    virtual ETLLoadBalancer&
    balancer() const = 0;

    virtual SubscriptionManager&
    subscriptions() const = 0;

    virtual Backend::BackendInterface&
    backend() const = 0;

    virtual NetworkValidatedLedgers&
    ledgers() const = 0;

    virtual DOSGuard&
    dosGuard() const = 0;

    virtual ReportingETL&
    reporting() const = 0;

    virtual RPC::WorkQueue&
    workers() const = 0;

    virtual void
    start() = 0;
};

class ApplicationImp : public Application
{
    mutable boost::asio::io_context rpcContext_;
    mutable boost::asio::io_context etlContext_;

    std::unique_ptr<Config> config_;

    mutable std::optional<boost::asio::ssl::context> sslContext_;

    std::unique_ptr<RPC::Counters> counters_;

    std::unique_ptr<RPC::WorkQueue> queue_;

    std::unique_ptr<DOSGuard> dosGuard_;

    std::unique_ptr<Backend::BackendInterface> backend_;

    // Manages clients subscribed to streams
    std::unique_ptr<SubscriptionManager> subscriptions_;

    // Tracks which ledgers have been validated by the
    // network
    std::unique_ptr<NetworkValidatedLedgers> ledgers_;

    // Handles the connection to one or more rippled nodes.
    // ETL uses the balancer to extract data.
    // The server uses the balancer to forward RPCs to a rippled node.
    // The balancer itself publishes to streams (transactions_proposed and
    // accounts_proposed)
    std::unique_ptr<ETLLoadBalancer> balancer_;

    // ETL is responsible for writing and publishing to streams. In read-only
    // mode, ETL only publishes
    std::unique_ptr<ReportingETL> etl_;

    // The server handles incoming RPCs
    std::shared_ptr<Server::HttpServer> httpServer_;

    std::optional<boost::asio::ssl::context>
    parseCerts(Config const& config);

    void
    initLogging(Config const& config);

public:
    ~ApplicationImp();

    ApplicationImp(std::unique_ptr<Config>&& config);

    Config const&
    config() const override
    {
        return *config_;
    }

    boost::asio::io_context&
    rpc() const override
    {
        return rpcContext_;
    }

    boost::asio::io_context&
    etl() const override
    {
        return etlContext_;
    }

    RPC::Counters&
    counters() const override
    {
        return *counters_;
    }

    std::optional<boost::asio::ssl::context>&
    sslContext() const override
    {
        return sslContext_;
    }

    ETLLoadBalancer&
    balancer() const override
    {
        return *balancer_;
    }

    SubscriptionManager&
    subscriptions() const override
    {
        return *subscriptions_;
    }

    Backend::BackendInterface&
    backend() const override
    {
        return *backend_;
    }

    NetworkValidatedLedgers&
    ledgers() const override
    {
        return *ledgers_;
    }

    DOSGuard&
    dosGuard() const override
    {
        return *dosGuard_;
    }

    ReportingETL&
    reporting() const override
    {
        return *etl_;
    }

    RPC::WorkQueue&
    workers() const override
    {
        return *queue_;
    }

    void
    start() override
    {
        initLogging(*config_);

        std::vector<std::thread> rpcWorkers = {};
        for (auto i = 0; i < config_->rpcWorkers; ++i)
            rpcWorkers.emplace_back([this]() { rpcContext_.run(); });

        std::vector<std::thread> etlWorkers = {};
        for (auto i = 0; i < config_->etlWorkers; ++i)
            rpcWorkers.emplace_back([this]() { etlContext_.run(); });

        etlContext_.run();

        for (auto& worker : rpcWorkers)
            worker.join();

        for (auto& worker : etlWorkers)
            worker.join();
    }
};

std::unique_ptr<Application>
make_Application(std::unique_ptr<Config> config);

#endif  // CLIO_APPLICATION_H