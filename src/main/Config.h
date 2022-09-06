#ifndef CLIO_CONFIG_H
#define CLIO_CONFIG_H

#include <boost/json.hpp>
#include <iostream>
#include <optional>
#include <variant>
#include <string>
#include <unordered_set>

struct ETLSourceConfig
{
    std::string ip;
    std::string wsPort;
    std::optional<std::string> grpcPort;
    std::vector<std::string> cacheCommands;
};

enum class CacheLoadStyle { ASYNC, SYNC, NOT_AT_ALL };

struct CacheConfig
{
    std::uint32_t numDiffs;
    CacheLoadStyle cacheLoadStyle;
};

struct ServerConfig
{
    std::string ip;
    std::uint32_t port;
};

struct DOSGuardConfig
{
    std::uint32_t maxFetches;
    std::uint32_t sweepInterval;
    std::unordered_set<std::string> whitelist;

private:
    static DOSGuardConfig
    parseDOSGuardConfig(boost::json::value& config);
};

struct MockDatabaseConfig
{
    std::string type = "mock";
};

struct CassandraConfig
{
    struct CassandraOptions
    {
        std::optional<std::string> secureConnectBundle;
        std::optional<std::string> contactPoints;
        std::optional<std::string> keyspace;
        std::optional<std::string> username;
        std::optional<std::string> password;
        std::optional<std::string> certfile;
        std::optional<std::uint32_t> port;
        std::optional<std::uint32_t> ttl;
        std::string tablePrefix;
        std::uint32_t replicationFactor;
        std::uint32_t syncInterval;
        std::uint32_t maxRequestsOutstanding;
        std::uint32_t threads;
    };

    CassandraOptions cassandra;

    std::string type = "cassandra";

    CassandraConfig(boost::json::value& options);
};

struct PostgresConfig
{
    struct PostgresOptions
    {
        bool experimental;
        bool rememberIp;
        std::string username;
        std::string password;
        std::string contactPoint;
        std::string database;
        std::uint32_t writeInterval;
        std::uint32_t timeout;
        std::uint32_t maxConnections;
    };

    PostgresOptions postgres;

    std::string type = "postgres";

    PostgresConfig(boost::json::value& options);
};

using DatabaseConfig = std::variant<CassandraConfig, PostgresConfig, MockDatabaseConfig>;

boost::json::object
parseConfig(std::string const& filename);

class Config
{
    boost::json::object json_;

public:
    DatabaseConfig database;
    DOSGuardConfig dosGuard;
    std::vector<ETLSourceConfig> etlSources;
    std::optional<CacheConfig> cache;
    std::optional<ServerConfig> server;

    bool readOnly;

    std::optional<std::string> sslCertFile;
    std::optional<std::string> sslKeyFile;

    std::string logLevel;
    bool logToConsole;
    std::optional<std::string> logDirectory;
    std::uint32_t logRotationSize;
    std::uint32_t logRotationHourInterval;
    std::uint32_t logDirectoryMaxSize;

    std::uint32_t numMarkers;

    std::uint32_t subscriptionWorkers;
    std::uint32_t etlWorkers;
    std::uint32_t rpcWorkers;
    std::uint32_t socketWorkers;
    std::uint32_t maxQueueSize;

    std::optional<std::uint32_t> startSequence;
    std::optional<std::uint32_t> finishSequence;
    std::optional<std::uint32_t> onlineDelete;
    std::uint32_t extractorThreads;
    std::uint32_t txnThreshold;

    // Be careful w/ these helper. They will throw if the underlying
    // database configuration is of the wrong type.
    PostgresConfig const&
    postgres() const
    {
        assert(std::holds_alternative<PostgresConfig>(database));

        return std::get<PostgresConfig>(database);
    }

    CassandraConfig const&
    cassandra() const
    {
        assert(std::holds_alternative<CassandraConfig>(database));

        return std::get<CassandraConfig>(database);
    }

    Config(boost::json::object const& json);
};

#endif  // CLIO_CONFIG_H