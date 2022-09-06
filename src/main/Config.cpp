#include <boost/algorithm/string.hpp>

#include <main/Config.h>

#include <fstream>
#include <iostream>

boost::json::object
parseConfig(std::string const& filename)
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

    throw std::runtime_error(
        "Could not parse JSON configuration. Verify it is valid JSON format");
}

struct cfgOPTIONAL;
struct cfgREQUIRED;

template <typename T, typename F>
void
add(T& value, boost::json::value& json, F&& parser)
{
    try
    {
        value = parser(json);
    }
    catch (std::runtime_error const& e)
    {
        throw std::runtime_error(
            "Could not parse field " + boost::json::serialize(json) + "\n" +
            e.what());
    }
}

template <typename T, typename F, typename Def>
void
add(T& value, boost::json::value& json, F&& parser, Def def)
{
    try
    {
        add(value, json, std::move(parser));
    }
    catch (std::exception const&)
    {
        value = def;
    }
}

template <typename T, typename F>
void
addPolymorphic(
    T& value,
    std::unique_ptr<T>& store,
    boost::json::value& json,
    F&& parser)
{
    add(store, json, parser);
    value = *store;
}

template <typename Required, typename T>
auto
checkRequired(std::optional<T>& potentiallyRequired)
{
    if constexpr (std::is_same<Required, cfgREQUIRED>::value)
    {
        if (!potentiallyRequired)
            throw std::runtime_error("Required value not present");

        return *potentiallyRequired;
    }
    else
    {
        return potentiallyRequired;
    }
}

template <typename Required = cfgOPTIONAL>
auto
parseString(boost::json::value& value)
{
    std::optional<std::string> parsed = {};

    if (value.is_string())
        parsed = value.as_string().c_str();

    return checkRequired<Required>(parsed);
}

std::string
parseString(boost::json::value& value, std::string dfault)
{
    auto possibleValue = parseString(value);

    if (!possibleValue)
        return dfault;

    return *possibleValue;
}

template<typename Required = cfgOPTIONAL>
auto
parseUInt32(boost::json::value& value)
{
    std::optional<std::uint32_t> parsed = {};

    if (value.is_int64())
        parsed = value.as_int64();

    return checkRequired<Required>(parsed);
}

std::uint32_t
parseUInt32(boost::json::value& value, std::uint32_t dfault)
{
    auto possibleValue = parseUInt32<cfgOPTIONAL>(value);

    if (!possibleValue)
        return dfault;

    return *possibleValue;
}

template<typename Required = cfgOPTIONAL>
auto
parseBool(boost::json::value& value)
{
    std::optional<bool> parsed = {};

    if (value.is_bool())
        parsed = value.as_bool();

    return checkRequired<Required>(parsed);
}

bool
parseBool(boost::json::value& value, bool dfault)
{
    auto optBool = parseBool<cfgOPTIONAL>(value);

    if (!optBool)
        return dfault;

    return *optBool;
}

template <typename T>
std::unordered_set<T>
parseSet(boost::json::value& value)
{
    if (!value.is_array())
        throw std::runtime_error("Collection must be an array");

    boost::json::array collection = value.as_array();

    std::unordered_set<T> result = {};
    std::transform(
        collection.begin(),
        collection.end(),
        std::inserter(result, result.begin()),
        [](boost::json::value& value) -> T { return value_to<T>(value); });

    return result;
}

DOSGuardConfig
parseDosGuardConfig(boost::json::value& value)
{
    if (value.is_null())
        return DOSGuardConfig{100, 1, {}};

    if (!value.is_object())
        throw std::runtime_error("DOSGuard config must be a json object");

    boost::json::object& config = value.as_object();

    DOSGuardConfig dosGuard;

    dosGuard.maxFetches = parseUInt32(config["max_fetches"], 100);
    dosGuard.sweepInterval = parseUInt32(config["sweep_interval"], 1);
    dosGuard.whitelist = parseSet<std::string>(config["whitelist"]);

    return dosGuard;
}

DatabaseConfig
parseDatabaseConfig(boost::json::value& config)
{
    if (!config.is_object())
        throw std::runtime_error("database config is not json object");

    auto& dbConfig = config.as_object();

    auto& type = config.at("type").as_string();

    if (type == "cassandra")
        return CassandraConfig(dbConfig[type]);
    else if (type == "postgres")
        return PostgresConfig(dbConfig[type]);
    else if (type == "mock")
        return MockDatabaseConfig();

    throw std::runtime_error("Unknown database type");
}

static std::vector<ETLSourceConfig>
parseETLSources(boost::json::value& config)
{
    std::vector<ETLSourceConfig> result;

    if (!config.is_array())
        throw std::runtime_error("etl_sources must be an array");

    auto& array = config.as_array();

    std::transform(
        array.begin(),
        array.end(),
        std::back_inserter(result),
        [](boost::json::value json) {
            if (!json.is_object())
                throw std::runtime_error("etl_source is not a json object");

            auto& object = json.as_object();

            ETLSourceConfig etl;
            etl.ip = parseString<cfgREQUIRED>(object["ip"]);
            etl.wsPort = parseString<cfgREQUIRED>(object["ws_port"]);
            etl.grpcPort = parseString(object["grpc_port"]);

            etl.cacheCommands = 
                [](boost::json::value& json) -> std::vector<std::string> {
                    if (json.is_null())
                        return {};

                    if (!json.is_array())
                        throw std::runtime_error(
                            "ETLSource `cache` is not an array");

                    std::vector<std::string> result = {};
                    for (auto const& cmd : json.as_array())
                    {
                        if (!cmd.is_string())
                            throw std::runtime_error(
                                "Cache command " + boost::json::serialize(cmd) +
                                " is not string");

                        result.push_back(cmd.as_string().c_str());
                    }

                    return result;
                }(object["cache"]);

            return etl;
        });

    return result;
}

std::optional<CacheConfig>
parseCache(boost::json::value& value)
{
    if (value.is_null())
        return {};

    if (!value.is_object())
        throw std::runtime_error("Cache config must be a json object");

    auto& cache = value.as_object();
    CacheConfig config;

    if (cache.contains("load") && cache.at("load").is_string())
    {
        auto entry = cache.at("load").as_string();
        boost::algorithm::to_lower(entry);

        if (entry == "sync")
            config.cacheLoadStyle = CacheLoadStyle::SYNC;
        else if (entry == "async")
            config.cacheLoadStyle = CacheLoadStyle::ASYNC;
        else if (entry == "none" || entry == "no")
            config.cacheLoadStyle = CacheLoadStyle::NOT_AT_ALL;
        else
            throw std::runtime_error(
                "Invalid cache load option: specify sync, async, or none");
    }

    config.numDiffs = parseUInt32(cache["num_diffs"], 1);

    return config;
}

ServerConfig
parseServerConfig(boost::json::value& value)
{
    if (value.is_null())
        return {"127.0.0.1", 51233};

    if (!value.is_object())
        throw std::runtime_error("Server config must be a json object");

    auto& object = value.as_object();

    ServerConfig config;
    config.ip = parseString(object["ip"], "127.0.0.1");
    config.port = parseUInt32(object["port"], 51233);

    return config;
}

CassandraConfig::CassandraConfig(boost::json::value& options)
{
    if (!options.is_object())
        throw std::runtime_error(
            "Cassandra database options are not an object");

    auto& object = options.as_object();

    cassandra.secureConnectBundle = parseString(object["secure_connect_bundle"]);
    cassandra.contactPoints = parseString(object["contact_points"]);

    cassandra.keyspace = parseString(object["keyspace"], "clio");
    cassandra.username = parseString(object["username"]);
    cassandra.password = parseString(object["password"]);
    cassandra.certfile = parseString(object["certfile"]);

    cassandra.maxRequestsOutstanding = parseUInt32(object["max_requests_outstanding"], 1000);
    cassandra.threads = parseUInt32(object["threads"], 2);
    cassandra.port = parseUInt32(object["port"]);

    cassandra.replicationFactor = parseUInt32(object["replication_factor"], 3);
    cassandra.syncInterval = parseUInt32(object["sync_interval"], 1);
    cassandra.tablePrefix = parseString(object["table_prefix"], "");
    cassandra.ttl = parseUInt32(object["ttl"]);
}

PostgresConfig::PostgresConfig(boost::json::value& options)
{
    if (!options.is_object())
        throw std::runtime_error("Postgres database options are not an object");

    auto& object = options.as_object();

    postgres.writeInterval = parseUInt32(object["write_interval"], 1000000);
    postgres.experimental = parseBool<cfgREQUIRED>(object["experimental"]);
    postgres.rememberIp = parseBool(object["remember_ip"], true);

    postgres.username = parseString<cfgREQUIRED>(object["username"]);
    postgres.password = parseString<cfgREQUIRED>(object["password"]);
    postgres.contactPoint = parseString<cfgREQUIRED>(object["contact_point"]);
    postgres.database = parseString<cfgREQUIRED>(object["database"]);

    for (auto& c : postgres.database)
        c = std::tolower(c);

    postgres.timeout = parseUInt32(object["timeout"], 600);
    postgres.maxConnections = parseUInt32(object["max_connections"], 1000);

}

Config::Config(boost::json::object const& json)
    : json_(json)
    , database(parseDatabaseConfig(json_["database"]))
    , dosGuard(parseDosGuardConfig(json_["dos_guard"]))
    , etlSources(parseETLSources(json_["etl_sources"]))
    , cache(parseCache(json_["cache"]))
    , server(parseServerConfig(json_["server"]))
    , readOnly(parseBool<cfgREQUIRED>(json_["read_only"]))
    , sslCertFile(parseString(json_["ssl_cert_file"]))
    , sslKeyFile(parseString(json_["ssl_key_file"]))
    , logLevel(parseString(json_["log_level"], "info"))
    , logToConsole(parseBool(json_["log_to_console"], true))
    , logDirectory(parseString(json_["log_directory"]))
    , logRotationSize(parseUInt32(json_["log_rotation_size"], 2 * 1024 * 1024 * 1024u))
    , logRotationHourInterval(parseUInt32(json_["log_rotation_hour_interval"], 12u))
    , logDirectoryMaxSize(parseUInt32(json_["log_directory_max_size"], 50 * 1024 * 1024 * 1024u))
    , numMarkers(parseUInt32(json_["num_markers"], 16))
    , subscriptionWorkers(parseUInt32(json_["subscription_workers"], 1))
    , etlWorkers(parseUInt32(json_["etl_workers"], 1))
    , rpcWorkers(parseUInt32(json_["rpc_workers"], 1))
    , socketWorkers(parseUInt32(json_["socket_workers"], 1))
    , maxQueueSize(parseUInt32(json_["max_queue_size"], std::numeric_limits<std::uint32_t>::max()))
    , startSequence(parseUInt32(json_["start_sequence"]))
    , finishSequence(parseUInt32(json_["finish_sequence"]))
    , extractorThreads(parseUInt32(json_["extractor_threads"], 1))
    , txnThreshold(parseUInt32(json_["txn_threshold"], 0))
{
}
