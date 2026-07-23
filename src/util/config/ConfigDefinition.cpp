#include "util/config/ConfigDefinition.hpp"

#include "rpc/common/APIVersion.hpp"
#include "util/Assert.hpp"
#include "util/Constants.hpp"
#include "util/OverloadSet.hpp"
#include "util/config/Array.hpp"
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigFileInterface.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Error.hpp"
#include "util/config/ObjectView.hpp"
#include "util/config/Types.hpp"
#include "util/config/ValueView.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace util::config {

ClioConfigDefinition::ClioConfigDefinition(std::initializer_list<KeyValuePair> pair)
{
    for (auto const& [key, value] : pair) {
        if (key.contains("[]"))
            ASSERT(std::holds_alternative<Array>(value), R"(Value must be array if key has "[]")");
        map_.insert({key, value});
    }
}

ObjectView
ClioConfigDefinition::getObject(std::string_view prefix, std::optional<std::size_t> idx) const
{
    auto const prefixWithDot = std::string(prefix) + ".";
    for (auto const& [mapKey, mapVal] : map_) {
        auto const hasPrefix = mapKey.starts_with(prefixWithDot);
        if (idx.has_value() && hasPrefix && std::holds_alternative<Array>(mapVal)) {
            ASSERT(std::get<Array>(mapVal).size() > *idx, "Index provided is out of scope");

            // we want to support getObject("array") and getObject("array.[]"), so we check if "[]"
            // exists
            if (!prefix.contains("[]"))
                return ObjectView{prefixWithDot + "[]", *idx, *this};
            return ObjectView{prefix, *idx, *this};
        }
        if (hasPrefix && !idx.has_value() && !mapKey.contains(prefixWithDot + "[]"))
            return ObjectView{prefix, *this};
    }
    ASSERT(false, "Key {} is not found in config", prefix);
    std::unreachable();
}

ArrayView
ClioConfigDefinition::getArray(std::string_view prefix) const
{
    auto const key = addBracketsForArrayKey(prefix);

    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(key)) {
            ASSERT(
                std::holds_alternative<Array>(mapVal),
                "Trying to retrieve Object or ConfigValue, instead of an Array "
            );
            return ArrayView{key, *this};
        }
    }
    ASSERT(false, "Key {} is not found in config", key);
    std::unreachable();
}

bool
ClioConfigDefinition::contains(std::string_view key) const
{
    return map_.contains(key);
}

bool
ClioConfigDefinition::hasItemsWithPrefix(std::string_view key) const
{
    return std::ranges::any_of(map_, [&key](auto const& pair) {
        return pair.first.starts_with(key);
    });
}

ValueView
ClioConfigDefinition::getValueView(std::string_view fullKey) const
{
    ASSERT(map_.contains(fullKey), "key {} does not exist in config", fullKey);
    if (std::holds_alternative<ConfigValue>(map_.at(fullKey))) {
        return ValueView{std::get<ConfigValue>(map_.at(fullKey))};
    }
    ASSERT(false, "Value of key {} is an Array, not an object", fullKey);
    std::unreachable();
}

std::chrono::milliseconds
ClioConfigDefinition::toMilliseconds(float value)
{
    ASSERT(value >= 0.0f, "Floating point value of seconds must be non-negative, got: {}", value);
    return std::chrono::milliseconds{
        std::lroundf(value * static_cast<float>(util::kMillisecondsPerSecond))
    };
}

ValueView
ClioConfigDefinition::getValueInArray(std::string_view fullKey, std::size_t index) const
{
    auto const it = getArrayIterator(fullKey);
    return ValueView{std::get<Array>(it->second).at(index)};
}

Array const&
ClioConfigDefinition::asArray(std::string_view fullKey) const
{
    auto const it = getArrayIterator(fullKey);
    return std::get<Array>(it->second);
}

std::size_t
ClioConfigDefinition::arraySize(std::string_view prefix) const
{
    auto const key = addBracketsForArrayKey(prefix);

    for (auto const& pair : map_) {
        if (pair.first.starts_with(key)) {
            return std::get<Array>(pair.second).size();
        }
    }
    ASSERT(false, "Prefix {} not found in any of the config keys", key);
    std::unreachable();
}

std::optional<std::vector<Error>>
ClioConfigDefinition::parse(ConfigFileInterface const& config)
{
    std::vector<Error> listOfErrors;
    std::unordered_map<std::string_view, std::vector<std::string_view>> arrayPrefixesToKeysMap;
    for (auto& [key, value] : map_) {
        if (key.contains(".[]")) {
            auto const prefix = Array::prefix(key);
            arrayPrefixesToKeysMap[prefix].push_back(key);
        }

        // if key doesn't exist in user config, makes sure it is marked as ".optional()" or has
        // ".defaultValue()"" in ClioConfigDefinition above
        if (!config.containsKey(key)) {
            if (std::holds_alternative<ConfigValue>(value)) {
                if (!(std::get<ConfigValue>(value).isOptional() ||
                      std::get<ConfigValue>(value).hasValue()))
                    listOfErrors.emplace_back(key, "key is required in user Config");
            }
            continue;
        }
        ASSERT(
            std::holds_alternative<ConfigValue>(value) || std::holds_alternative<Array>(value),
            "Value must be of type ConfigValue or Array"
        );
        std::visit(
            util::OverloadSet{
                // handle the case where the config value is a single element.
                // attempt to set the value from the configuration for the specified key.
                [&key, &config, &listOfErrors](ConfigValue& val) {
                    if (auto const maybeError = val.setValue(config.getValue(key), key);
                        maybeError.has_value()) {
                        listOfErrors.emplace_back(*maybeError);
                    }
                },
                // handle the case where the config value is an array.
                // iterate over each provided value in the array and attempt to set it for the key.
                [&key, &config, &listOfErrors](Array& arr) {
                    for (auto const& val : config.getArray(key)) {
                        if (val.has_value()) {
                            if (auto const maybeError = arr.addValue(*val, key);
                                maybeError.has_value()) {
                                listOfErrors.emplace_back(*maybeError);
                            }
                        } else {
                            if (auto const maybeError = arr.addNull(key); maybeError.has_value()) {
                                listOfErrors.emplace_back(*maybeError);
                            }
                        }
                    }
                }
            },
            value
        );
    }

    if (!listOfErrors.empty())
        return listOfErrors;

    // The code above couldn't detect whether some fields in an array are missing.
    // So to fix it for each array we determine it's size and add empty values if the field is
    // optional or generate an error.
    for (auto const& [_, keys] : arrayPrefixesToKeysMap) {
        size_t maxSize = 0;
        std::ranges::for_each(keys, [&](std::string_view key) {
            ASSERT(std::holds_alternative<Array>(map_.at(key)), "{} is not array", key);
            maxSize = std::max(maxSize, arraySize(key));
        });
        if (maxSize == 0) {
            // empty arrays are allowed
            continue;
        }

        std::ranges::for_each(keys, [&](std::string_view key) {
            auto& array = std::get<Array>(map_.at(key));
            while (array.size() < maxSize) {
                auto const err = array.addNull(key);
                if (err.has_value()) {
                    listOfErrors.emplace_back(*err);
                    break;
                }
            }
        });
    }

    for (auto const& key : config.getAllKeys()) {
        if (!map_.contains(key) && !arrayPrefixesToKeysMap.contains(key)) {
            listOfErrors.emplace_back("Unknown key: " + key);
        }
    }

    if (!listOfErrors.empty())
        return listOfErrors;

    return std::nullopt;
}

ClioConfigDefinition&
getClioConfig()
{
    static ClioConfigDefinition kClioConfig{
        {{"database.type",
          ConfigValue{ConfigType::String}
              .defaultValue("cassandra")
              .withConstraint(gValidateCassandraName)},
         {"database.cassandra.contact_points",
          ConfigValue{ConfigType::String}.defaultValue("localhost")},
         {"database.cassandra.secure_connect_bundle", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.port",
          ConfigValue{ConfigType::Integer}.withConstraint(gValidatePort).optional()},
         {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue("clio")},
         {"database.cassandra.replication_factor",
          ConfigValue{ConfigType::Integer}.defaultValue(3u).withConstraint(
              gValidateReplicationFactor
          )},
         {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.max_write_requests_outstanding",
          ConfigValue{ConfigType::Integer}.defaultValue(10'000).withConstraint(gValidateUint32)},
         {"database.cassandra.max_read_requests_outstanding",
          ConfigValue{ConfigType::Integer}.defaultValue(100'000).withConstraint(gValidateUint32)},
         {"database.cassandra.threads",
          ConfigValue{ConfigType::Integer}
              .defaultValue(
                  static_cast<uint32_t>(std::thread::hardware_concurrency()),
                  "The number of available CPU cores."
              )
              .withConstraint(gValidateUint32)},
         {"database.cassandra.core_connections_per_host",
          ConfigValue{ConfigType::Integer}.defaultValue(1).withConstraint(gValidateUint16)},
         {"database.cassandra.queue_size_io",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint16)},
         {"database.cassandra.write_batch_size",
          ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(gValidateUint16)},
         {"database.cassandra.connect_timeout",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint32)},
         {"database.cassandra.request_timeout",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint32)},
         {"database.cassandra.username", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.password", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.certfile", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.provider",
          ConfigValue{ConfigType::String}
              .defaultValue("cassandra")
              .withConstraint(gValidateProvider)},

         {"allow_no_etl", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
         {"etl_sources.[].ip",
          Array{ConfigValue{ConfigType::String}.optional().withConstraint(gValidateIp)}},
         {"etl_sources.[].ws_port",
          Array{ConfigValue{ConfigType::String}.optional().withConstraint(gValidatePort)}},
         {"etl_sources.[].grpc_port",
          Array{ConfigValue{ConfigType::String}.optional().withConstraint(gValidatePort)}},

         {"forwarding.cache_timeout",
          ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(
              gValidatePositiveDouble
          )},
         {"forwarding.request_timeout",
          ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(
              gValidatePositiveDouble
          )},

         {"rpc.cache_timeout",
          ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(
              gValidatePositiveDouble
          )},

         {"num_markers",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateNumMarkers)},

         {"dos_guard.whitelist.[]",
          Array{ConfigValue{ConfigType::String}.optional().withConstraint(gValidateIp)}},
         {"dos_guard.max_fetches",
          ConfigValue{ConfigType::Integer}.defaultValue(1000'000u).withConstraint(gValidateUint32)},
         {"dos_guard.max_connections",
          ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(gValidateUint32)},
         {"dos_guard.max_requests",
          ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(gValidateUint32)},
         {"dos_guard.sweep_interval",
          ConfigValue{ConfigType::Double}.defaultValue(1.0).withConstraint(
              gValidatePositiveDouble
          )},
         {"dos_guard.__ng_default_weight",
          ConfigValue{ConfigType::Integer}.defaultValue(1).withConstraint(
              gValidateNonNegativeUint32
          )},
         {"dos_guard.__ng_weights.[].method",
          Array{ConfigValue{ConfigType::String}.withConstraint(gRpcNameConstraint)}},
         {"dos_guard.__ng_weights.[].weight",
          Array{ConfigValue{ConfigType::Integer}.withConstraint(gValidateNonNegativeUint32)}},
         {"dos_guard.__ng_weights.[].weight_ledger_current",
          Array{
              ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateNonNegativeUint32)
          }},
         {"dos_guard.__ng_weights.[].weight_ledger_validated",
          Array{
              ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateNonNegativeUint32)
          }},

         {"workers",
          ConfigValue{ConfigType::Integer}
              .defaultValue(
                  std::thread::hardware_concurrency(), "The number of available CPU cores."
              )
              .withConstraint(gValidateUint32)},
         {"server.ip", ConfigValue{ConfigType::String}.withConstraint(gValidateIp)},
         {"server.port", ConfigValue{ConfigType::Integer}.withConstraint(gValidatePort)},
         {"server.max_queue_size",
          ConfigValue{ConfigType::Integer}.defaultValue(1000).withConstraint(gValidateUint32)},
         {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
         {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
         {"server.processing_policy",
          ConfigValue{ConfigType::String}
              .defaultValue("parallel")
              .withConstraint(gValidateProcessingPolicy)},
         {"server.parallel_requests_limit",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint16)},
         {"server.ws_max_sending_queue_size",
          ConfigValue{ConfigType::Integer}.defaultValue(1500).withConstraint(gValidateUint32)},
         {"server.__ng_web_server", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
         {"server.proxy.ips.[]",
          Array{ConfigValue{ConfigType::String}.withConstraint(gValidateIp)}},
         {"server.proxy.tokens.[]", Array{ConfigValue{ConfigType::String}}},

         {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
         {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},

         {"io_threads",
          ConfigValue{ConfigType::Integer}.defaultValue(2).withConstraint(gValidateUint16)},

         {"subscription_workers",
          ConfigValue{ConfigType::Integer}.defaultValue(1).withConstraint(gValidateUint32)},

         {"graceful_period",
          ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(
              gValidatePositiveDouble
          )},

         {"cache.num_diffs",
          ConfigValue{ConfigType::Integer}.defaultValue(32).withConstraint(gValidateUint16)},
         {"cache.num_markers",
          ConfigValue{ConfigType::Integer}.defaultValue(48).withConstraint(gValidateUint16)},
         {"cache.num_cursors_from_diff",
          ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(gValidateNumCursors)},
         {"cache.num_cursors_from_account",
          ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(gValidateNumCursors)},
         {"cache.page_fetch_size",
          ConfigValue{ConfigType::Integer}.defaultValue(512).withConstraint(gValidateUint16)},
         {"cache.limit_load_in_cluster", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
         {"cache.load",
          ConfigValue{ConfigType::String}.defaultValue("async").withConstraint(gValidateLoadMode)},
         {"cache.file.path", ConfigValue{ConfigType::String}.optional()},
         {"cache.file.max_sequence_age", ConfigValue{ConfigType::Integer}.defaultValue(5000)},
         {"cache.file.async_save", ConfigValue{ConfigType::Boolean}.defaultValue(false)},

         {"log.channels.[].channel",
          Array{ConfigValue{ConfigType::String}.optional().withConstraint(gValidateChannelName)}},
         {"log.channels.[].level",
          Array{ConfigValue{ConfigType::String}.optional().withConstraint(gValidateLogLevelName)}},

         {"log.level",
          ConfigValue{ConfigType::String}.defaultValue("info").withConstraint(
              gValidateLogLevelName
          )},

         {"log.format",
          ConfigValue{ConfigType::String}.defaultValue(R"(%Y-%m-%d %H:%M:%S.%f %^%3!l:%n%$ - %v)")},

         {"log.is_async", ConfigValue{ConfigType::Boolean}.defaultValue(true)},

         {"log.enable_console", ConfigValue{ConfigType::Boolean}.defaultValue(false)},

         {"log.directory", ConfigValue{ConfigType::String}.optional()},

         {"log.rotation_size",
          ConfigValue{ConfigType::Integer}.defaultValue(2048).withConstraint(gValidateUint32)},

         {"log.directory_max_files",
          ConfigValue{ConfigType::Integer}.defaultValue(25).withConstraint(gValidateUint32)},

         {"log.rotate", ConfigValue{ConfigType::Boolean}.defaultValue(true)},

         {"log.tag_style",
          ConfigValue{ConfigType::String}.defaultValue("none").withConstraint(gValidateLogTag)},

         {"extractor_threads",
          ConfigValue{ConfigType::Integer}.defaultValue(1u).withConstraint(gValidateUint32)},

         {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)},

         {"start_sequence",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint32)},

         {"finish_sequence",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint32)},

         {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},

         {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},

         {"api_version.default",
          ConfigValue{ConfigType::Integer}
              .defaultValue(rpc::kApiVersionDefault)
              .withConstraint(gValidateApiVersion)},
         {"api_version.min",
          ConfigValue{ConfigType::Integer}
              .defaultValue(rpc::kApiVersionMin)
              .withConstraint(gValidateApiVersion)},
         {"api_version.max",
          ConfigValue{ConfigType::Integer}
              .defaultValue(rpc::kApiVersionMax)
              .withConstraint(gValidateApiVersion)},

         {"migration.full_scan_threads",
          ConfigValue{ConfigType::Integer}.defaultValue(2).withConstraint(gValidateUint32)},
         {"migration.full_scan_jobs",
          ConfigValue{ConfigType::Integer}.defaultValue(4).withConstraint(gValidateUint32)},
         {"migration.cursors_per_job",
          ConfigValue{ConfigType::Integer}.defaultValue(100).withConstraint(gValidateUint32)}},
    };

    return kClioConfig;
}

}  // namespace util::config
