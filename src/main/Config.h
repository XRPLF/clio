#ifndef CLIO_CONFIG_H
#define CLIO_CONFIG_H

#include <boost/json.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>

struct ETLSourceConfig
{
    std::string ip;
    std::string wsPort;
    std::string grpcPort;
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

struct DatabaseConfig
{
    std::string type;

    virtual ~DatabaseConfig(){};

    static DatabaseConfig
    parseDatabaseConfig(boost::json::value& cofig);
};

struct MockDatabaseConfig : public DatabaseConfig
{
    MockDatabaseConfig()
    {
        type = "mock";
    }
};

struct CassandraConfig : public DatabaseConfig
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

    CassandraConfig(boost::json::value& config);
};

struct PostgresConfig : public DatabaseConfig
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

    PostgresConfig(boost::json::value& config);
};

boost::json::object
parseConfig(std::string const& filename);

class Config
{
    boost::json::object json_;
    std::shared_ptr<DatabaseConfig> dbConfigStore;

public:
    DatabaseConfig& database;
    DOSGuardConfig dosGuard;
    std::vector<ETLSourceConfig> etlSources;
    std::optional<CacheConfig> cache;
    std::optional<ServerConfig> server;

    bool readOnly;

    std::optional<std::string> sslCertFile;
    std::optional<std::string> sslKeyFile;

    std::optional<std::string> logFile;
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
        return dynamic_cast<PostgresConfig const&>(database);
    }

    CassandraConfig const&
    cassandra() const
    {
        return dynamic_cast<CassandraConfig const&>(database);
    }

    Config(boost::json::object const& json);
};

#endif  // CLIO_CONFIG_H