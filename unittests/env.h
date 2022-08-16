#include <backend/BackendFactory.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <main/Application.h>
#include <webserver/WsBase.h>

struct cfgPOSTGRES
{
};
struct cfgCASSANDRA
{
};
using AllBackends = ::testing::Types<cfgCASSANDRA, cfgPOSTGRES>;

auto constexpr cassandraJson = [](std::string const& keyspace) {
    boost::json::object obj = boost::json::parse(
                                  R"({
        "database": {
            "type": "cassandra",
            "cassandra": {
                "contact_points": "127.0.0.1",
                "port": 9042,
                "replication_factor": 1,
                "table_prefix": "",
                "max_requests_outstanding": 1000
            }
        },
        "etl_sources": [
            { "ip": "0.0.0.0", "ws_port": "6005", "grpc_port": "50051" }
        ],
        "read_only": false
    })")
                                  .as_object();

    obj["database"].as_object()["cassandra"].as_object()["keyspace"] = keyspace;

    return std::make_unique<Config>(obj);
};

auto constexpr postgresJson = [](std::string const& keyspace) {
    boost::json::object obj = boost::json::parse(
                                  R"({
        "database": {
            "type": "postgres",
            "postgres": {
                "experimental": true,
                "contact_point": "127.0.0.1",
                "username": "postgres",
                "password": "postgres",
                "max_connections": 10
            }
        },
        "etl_sources": [
            { "ip": "0.0.0.0", "ws_port": "6005", "grpc_port": "50051" }
        ],
        "read_only": false
    })")
                                  .as_object();

    obj["database"].as_object()["postgres"].as_object()["database"] = keyspace;

    return std::make_unique<Config>(obj);
};

template <typename ConfigType>
std::unique_ptr<Config>
getConfig(std::string const& keyspace)
{
    if constexpr (std::is_same<ConfigType, cfgCASSANDRA>::value)
        return cassandraJson(keyspace);
    else if constexpr (std::is_same<ConfigType, cfgPOSTGRES>::value)
        return postgresJson(keyspace);
    else
        return nullptr;
}

class MockSubscriber : public WsBase
{
public:
    MockSubscriber()
    {
    }

    ~MockSubscriber() override
    {
    }

    MOCK_METHOD(void, send, (std::shared_ptr<Message>), (override));
    MOCK_METHOD(bool, dead, (), ());
};

class MockApplication : public Application
{
    mutable boost::asio::io_context rpc_{};
    mutable boost::asio::io_context etl_{};
    mutable boost::asio::io_context socket_{};

    std::unique_ptr<Config> config_;
    std::unique_ptr<Backend::BackendInterface> backend_;

public:
    MockApplication(std::unique_ptr<Config>&& config)
        : config_(std::move(config)), backend_(Backend::make_Backend(*this))
    {
    }

    boost::asio::io_context&
    socketIoc() const override
    {
        return socket_;
    }

    boost::asio::io_context&
    etlIoc() const override
    {
        return etl_;
    }

    boost::asio::io_context&
    rpcIoc() const override
    {
        return rpc_;
    }

    Config const&
    config() const override
    {
        return *config_;
    }

    Backend::BackendInterface&
    backend() const override
    {
        return *backend_;
    }

    MOCK_METHOD(RPC::Counters&, counters, (), (const, override));
    MOCK_METHOD(
        std::optional<boost::asio::ssl::context>&,
        sslContext,
        (),
        (const, override));
    MOCK_METHOD(ETLLoadBalancer&, balancer, (), (const, override));
    MOCK_METHOD(SubscriptionManager&, subscriptions, (), (const, override));
    MOCK_METHOD(
        NetworkValidatedLedgers&,
        networkValidatedLedgers,
        (),
        (const, override));
    MOCK_METHOD(DOSGuard&, dosGuard, (), (const, override));
    MOCK_METHOD(ReportingETL&, reportingETL, (), (const, override));
    MOCK_METHOD(RPC::WorkQueue&, workQueue, (), (const, override));
    MOCK_METHOD(void, start, (), (override));
};

template <typename ConfigType>
class Clio : public ::testing::TestWithParam<ConfigType>
{
private:
    const ::testing::TestInfo* const test_info_ =
        ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name_ = std::string{test_info_->name()};

    std::string time_string_ = std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());

    std::string keyspace_;
    MockApplication app_;

protected:
    Clio()
        : keyspace_("clio_test_" + test_name_ + "_" + time_string_)
        , app_(getConfig<ConfigType>(keyspace_))
    {
    }

    ~Clio() override
    {
    }

public:
    std::string
    keyspace()
    {
        return keyspace_;
    }

    MockApplication&
    app()
    {
        return app_;
    }
};