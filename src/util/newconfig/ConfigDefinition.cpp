//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "util/newconfig/ConfigDefinition.hpp"

#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Object.hpp"

#include <array>
#include <cstddef>

namespace util::config {

/**
 * @brief Full Clio Configuration definition. Specifies which keys it accepts, which fields are mandatory and which
 * fields have default values
 */
Object ClioConfig{{
    {"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
    {"database.cassandra.contact_points", ConfigValue{ConfigType::String}.defaultValue("localhost")},
    {"database.cassandra.port", ConfigValue{ConfigType::Integer}.defaultValue(2)},
    {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue("localhost")},
    {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(1)},
    {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.defaultValue("localhost")},
    {"database.cassandra.max_write_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(23)},
    {"database.cassandra.max_read_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(23)},
    {"database.cassandra.threads", ConfigValue{ConfigType::Integer}.defaultValue(23)},
    {"etl_source.[].ip", ConfigValue{ConfigType::String}.required()},
    {"etl_source.[].ws_port", ConfigValue{ConfigType::String}.required()},
    {"etl_source.[].grpc_port", ConfigValue{ConfigType::String}.required()},
    {"forwarding_cache_timeout", ConfigValue{ConfigType::String}.required()},
    {"dos_guard.whitelist", ConfigValue{ConfigType::String}.required()},
    {"dos_guard.max_fetches", ConfigValue{ConfigType::String}.required()},
    {"dos_guard.max_connections", ConfigValue{ConfigType::String}.required()},
    {"dos_guard.max_requests", ConfigValue{ConfigType::String}.required()},
    {"dos_guard.sweep_interval", ConfigValue{ConfigType::String}.required()},
    {"cache.peers.[].ip", ConfigValue{ConfigType::String}.required()},
    {"cache.peers.[].port", ConfigValue{ConfigType::String}.required()},
    {"server.ip", ConfigValue{ConfigType::String}.required()},
    {"server.port", ConfigValue{ConfigType::String}.required()},
    {"server.max_queue_size", ConfigValue{ConfigType::String}.required()},
    {"server.local_admin", ConfigValue{ConfigType::String}.required()},
    {"prometheus.compress_reply", ConfigValue{ConfigType::String}.required()},
}};

constexpr ClioConfigDescription const DESCRIPTIONS{std::array<ClioConfigDescription::KV, 4>{
    {{"database", "Config for database"},
     {"database.type", "Type of database"},
     {"database.cassandra.contact_points", "Contact points for Cassandra"},
     {"etl_source.[].ip", "IP address for ETL source"}}
}};

/*
static Object ClioConfigDefinition = Object{
    {
        "database", Object{
            {
                {"type", ConfigValue{ConfigType::String}.required()},
                {"cassandra", Object{
                    {
                        {"contact_points", ConfigValue{ConfigType::String}.required()},
                        {"port", ConfigValue{ConfigType::Integer}},
                        {"keyspace", ConfigValue{ConfigType::String}.defaultValue("clio")},
                        {"replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(3)},
                        {"table_prefix", ConfigValue{ConfigType::String}},  // doesn't have defaultValue or required()
                        is optional for user to include
                        {"max_write_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(10'000)},
                        {"max_read_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(100'000)},
                        {"threads", ConfigValue{ConfigType::String}.defaultValue(std::thread::hardware_concurrency())},
                        {"core_connections_per_host", ConfigValue{ConfigType::String}.defaultValue(1)}
                    }
                }}
            }
        }
    },
    {
        "etl_sources", Array{
            Object{
                {
                    {"ip", ConfigValue{ConfigType::String}.defaultValue("")},
                    {"ws_port", ConfigValue{ConfigType::String}.defaultValue("")},
                    {"grpc_port", ConfigValue{ConfigType::String}.defaultValue("")}
                }
            }
        }
    },
    {"forwarding_cache_timeout", ConfigValue{ConfigType::String}.defaultValue(0)},
    {
        "dos_guard", Object{
            {
                {"whitelist", Array{Object{{}}}},
                {"max_fetches", ConfigValue{1000000}},
                {"max_connections", ConfigValue{20}},
                {"max_requests", ConfigValue{20}},
                {"sweep_interval", ConfigValue{1}}
            }
        }
    },
    {
        "cache", Object{
            {
                {"peers", Array{
                    Object{
                        {{"ip", ConfigValue{"127.0.0.1"}}, {"port", ConfigValue{"123"}}}
                    }
                }}
            }
        }
    },
    {
        "server", Object{
            {
                {"ip", ConfigValue{"IP address of the server"}.defaultValue("0.0.0.0")},
                {"port", ConfigValue{51555}},
                {"max_queue_size", ConfigValue{500}},
                {"local_admin", ConfigValue{true}}
            }
        }
    },
    {"prometheus", Object{
        {{"compress_reply", ConfigValue{true}}}
    }},
    {
        "log_channels", Array{
            Object{
                {{"channel", ConfigValue{"WebServer"}}, {"log_level", ConfigValue{"info"}}}
            },
            Object{
                {{"channel", ConfigValue{"WebServer"}}, {"log_level", ConfigValue{"info"}}}
            },
            Object{
                {{"channel", ConfigValue{"Subscriptions"}}, {"log_level", ConfigValue{"info"}}}
            },
            Object{
                {{"channel", ConfigValue{"RPC"}}, {"log_level", ConfigValue{"info"}}}
            },
            Object{
                {{"channel", ConfigValue{"ETL"}}, {"log_level", ConfigValue{"info"}}}
            },
            Object{
                {{"channel", ConfigValue{"Performance"}}, {"log_level", ConfigValue{"info"}}}
            }
        }
    },
    {"log_level", ConfigValue{"info"}},
    {"log_format", ConfigValue{R"("%TimeStamp% (%SourceLocation%) [%ThreadID%] %Channel%:%Severity% %Message%)"}},
    {"log_to_console", ConfigValue{true}},
    {"log_directory", ConfigValue{"./clio_log"}},
    {"log_rotation_size", ConfigValue{2048}},
    {"log_directory_max_size", ConfigValue{51200}},
    {"log_rotation_hour_interval", ConfigValue{12}},
    {"log_tag_style", ConfigValue{"uint"}},
    {"extractor_threads", ConfigValue{2}},
    {"read_only", ConfigValue{false}},
    {"start_sequence", ConfigValue{""}},
    {"finish_sequence", ConfigValue{""}},
    {"ssl_cert_file", ConfigValue{""}},
    {"ssl_key_file", ConfigValue{""}},
    {"api_version", Object{
        {{"min", ConfigValue{1}}, {"max", ConfigValue{2}}}
    }}
};
*/

/*
std::optional<Error> ConfigFileDefinition::parse(ConfigFileInterface const& config)
{
    // TODO: use get value/get Array to compare what is needed;
}
*/

}  // namespace util::config
