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

std::optional<std::string>
parseString(boost::json::value& value)
{
    std::optional<std::string> parsed = {};

    if (value.is_string())
        parsed = value.as_string().c_str();

    return parsed;
}

std::optional<std::string>
parseString(boost::json::value& value, std::string dfault)
{
    auto possibleValue = parseString(value);

    if (!possibleValue)
        return dfault;

    return *possibleValue;
}

std::optional<std::uint32_t>
parseUInt32(boost::json::value& value)
{
    std::optional<std::uint32_t> parsed = {};

    if (value.is_int64())
        parsed = value.as_int64();

    return parsed;
}

std::uint32_t
parseUInt32(boost::json::value& value, std::uint32_t dfault)
{
    auto possibleValue = parseUInt32(value);

    if (!possibleValue)
        return dfault;

    return *possibleValue;
}

std::optional<bool>
parseBool(boost::json::value& value)
{
    std::optional<bool> parsed = {};

    if (value.is_bool())
        parsed = value.as_bool();

    return parsed;
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

static DOSGuardConfig
parseDosGuardConfig(boost::json::value& value)
{
    if (value.is_null())
        return DOSGuardConfig{100, 1, {}};

    if (!value.is_object())
        throw std::runtime_error("DOSGuard config must be a json object");

    boost::json::object& config = value.as_object();

    DOSGuardConfig dosGuard;
    // add(dosGuard.maxFetches,
    //     config["max_fetches"],
    //     &parseUInt32<cfgREQUIRED>,
    //     100);
    // add(dosGuard.sweepInterval,
    //     config["sweep_interval"],
    //     &parseUInt32<cfgREQUIRED>,
    //     1);
    // add(dosGuard.whitelist, config["whitelist"], &parseSet<std::string>);

    return dosGuard;
}

static std::unique_ptr<DatabaseConfig>
parseDatabaseConfig(boost::json::value& config)
{
    if (!config.is_object())
        throw std::runtime_error("database config is not json object");

    auto& dbConfig = config.as_object();

    auto& type = config.at("type").as_string();

    if (type == "cassandra")
        return std::make_unique<CassandraConfig>(dbConfig[type]);
    else if (type == "postgres")
        return std::make_unique<PostgresConfig>(dbConfig[type]);
    else if (type == "mock")
        return std::make_unique<MockDatabaseConfig>();

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
            // add(etl.ip, object["ip"], &parseString<cfgREQUIRED>);
            // add(etl.wsPort, object["ws_port"], &parseString<cfgREQUIRED>);
            // add(etl.grpcPort, object["grpc_port"], &parseString<cfgREQUIRED>);
            // add(
            //     etl.cacheCommands,
            //     object["cache"],
            //     [](boost::json::value& json) -> std::vector<std::string> {
            //         if (!json.is_array())
            //             throw std::runtime_error(
            //                 "ETLSource `cache` is not an array");

            //         std::vector<std::string> result = {};
            //         for (auto const& cmd : json.as_array())
            //         {
            //             if (!cmd.is_string())
            //                 throw std::runtime_error(
            //                     "Cache command " + boost::json::serialize(cmd) +
            //                     " is not string");

            //             result.push_back(cmd.as_string().c_str());
            //         }

            //         return result;
            //     },
            //     std::vector<std::string>{});
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

    // add(config.numDiffs, cache["num_diffs"], &parseUInt32<cfgREQUIRED>, 1);

    return config;
}

ServerConfig
parseServerConfig(boost::json::value& value)
{
    if (value.is_null())
        return {};

    if (!value.is_object())
        throw std::runtime_error("Server config must be a json object");

    auto& object = value.as_object();

    ServerConfig config;
    // add(config.ip, object["ip"], &parseString<cfgREQUIRED>);
    // add(config.port, object["port"], &parseUInt32<cfgREQUIRED>);

    return config;
}

CassandraConfig::CassandraConfig(boost::json::value& options)
{
    if (!options.is_object())
        throw std::runtime_error(
            "Cassandra database options are not an object");

    auto& object = options.as_object();

    type = "cassandra";

    // add(cassandra.secureConnectBundle,
    //     object["secure_connect_bundle"],
    //     &parseString<cfgOPTIONAL>);
    // add(cassandra.contactPoints,
    //     object["contact_points"],
    //     &parseString<cfgOPTIONAL>);
    // add(cassandra.keyspace, object["keyspace"], &parseString<cfgREQUIRED>);
    // add(cassandra.username, object["username"], &parseString<cfgOPTIONAL>);
    // add(cassandra.password, object["password"], &parseString<cfgOPTIONAL>);
    // add(cassandra.certfile, object["certfile"], &parseString<cfgOPTIONAL>);
    // add(cassandra.maxRequestsOutstanding,
    //     object["max_requests_outstanding"],
    //     &parseUInt32<cfgREQUIRED>,
    //     1000);
    // add(cassandra.threads, object["threads"], &parseUInt32<cfgREQUIRED>, 2);
    // add(cassandra.port, object["port"], &parseUInt32<cfgOPTIONAL>);
    // add(cassandra.replicationFactor,
    //     object["replication_factor"],
    //     &parseUInt32<cfgREQUIRED>,
    //     3);
    // add(cassandra.syncInterval,
    //     object["sync_interval"],
    //     &parseUInt32<cfgREQUIRED>,
    //     1);
    // add(cassandra.tablePrefix,
    //     object["table_prefix"],
    //     &parseString<cfgREQUIRED>,
    //     "");
    // add(cassandra.ttl, object["ttl"], &parseUInt32<cfgOPTIONAL>);
}

PostgresConfig::PostgresConfig(boost::json::value& options)
{
    if (!options.is_object())
        throw std::runtime_error("Postgres database options are not an object");

    auto& object = options.as_object();

    type = "postgres";

    // add(postgres.writeInterval,
    //     object["write_interval"],
    //     &parseUInt32<cfgREQUIRED>,
    //     1000000);
    // add(postgres.experimental, object["experimental"], &parseBool<cfgREQUIRED>);
    // add(postgres.rememberIp,
    //     object["remember_ip"],
    //     &parseBool<cfgREQUIRED>,
    //     true);
    // add(postgres.username, object["username"], &parseString<cfgREQUIRED>);
    // add(postgres.password, object["password"], &parseString<cfgREQUIRED>);
    // add(postgres.contactPoint,
    //     object["contact_point"],
    //     &parseString<cfgREQUIRED>);
    // add(postgres.database, object["database"], &parseString<cfgREQUIRED>);

    // for (auto& c : postgres.database)
    //     c = std::tolower(c);

    // add(postgres.timeout, object["timeout"], &parseUInt32<cfgREQUIRED>, 600);
    // add(postgres.maxConnections,
    //     object["max_connections"],
    //     &parseUInt32<cfgREQUIRED>,
    //     1000);
}

Config::Config(boost::json::object const& json)
    : json_(json)
    , dbConfigStore(parseDatabaseConfig(json_["database"]))
    , database(*dbConfigStore)
    , dosGuard(parseDosGuardConfig(json_["dos_guard"]))
    , etlSources(parseETLSources(json_["etl_sources"]))
    , cache(parseCache(json_["cache"]))
    , server(parseServerConfig(json_["server"]))
    , readOnly(parseBool(json_["read_only"]))
    , sslCertFile(parseString(json_["ssl_cert_file"]))
    , sslKeyFile(parseString(json_["ssl_key_file"]))
    , subscriptionWorkers(parseUInt32(json_["subscription_workers"], 1))
{
//     add(etlWorkers, json_["etl_workers"], &parseUInt32<cfgREQUIRED>, 1);
//     add(rpcWorkers, json_["rpc_workers"], &parseUInt32<cfgREQUIRED>, 1);
//     add(socketWorkers, json_["socket_workers"], &parseUInt32<cfgREQUIRED>, 1);
//     add(maxQueueSize,
//         json_["max_queue_size"],
//         &parseUInt32<cfgREQUIRED>,
//         std::numeric_limits<std::uint32_t>::max());
//     add(numMarkers, json_["num_ranges"], &parseUInt32<cfgREQUIRED>, 16);

//     add(logFile, json_["log_file"], &parseString<cfgOPTIONAL>);
//     add(logLevel, json_["log_level"], &parseString<cfgREQUIRED>, "info");
//     add(logRotationSize,
//         json_["log_rotation_size"],
//         &parseUInt32<cfgREQUIRED>,
//         2 * 1024 * 1024 * 1024u);
//     add(logRotationHourInterval,
//         json_["log_rotation_hour_interval"],
//         &parseUInt32<cfgREQUIRED>,
//         12u);
//     add(logDirectoryMaxSize,
//         json_["log_directory_max_size"],
//         &parseUInt32<cfgREQUIRED>,
//         50 * 1024 * 1024 * 1024u);

//     add(startSequence, json_["start_sequence"], &parseUInt32<cfgOPTIONAL>);
//     add(finishSequence, json_["finish_sequence"], &parseUInt32<cfgOPTIONAL>);
//     add(onlineDelete, json_["online_delete"], &parseUInt32<cfgOPTIONAL>);
//     add(extractorThreads,
//         json_["extractor_threads"],
//         &parseUInt32<cfgREQUIRED>,
//         1);
//     add(txnThreshold, json_["txn_threshold"], &parseUInt32<cfgREQUIRED>, 0);
}
