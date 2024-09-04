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

#pragma once

#include "util/Assert.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/json/value.hpp>

#include <thread>

using namespace util::config;

// config used for load balancer test
inline ClioConfigDefinition
getParseLoadBalancerConfig(boost::json::value jsonValues)
{
    ClioConfigDefinition config{
        {{"forwarding.cache_timeout",
          ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(ValidatePositiveDouble)},
         {"forwarding.request_timeout",
          ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(ValidatePositiveDouble)},
         {"allow_no_etl", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
         {"etl_sources.[].ip", Array{ConfigValue{ConfigType::String}.optional().withConstraint(validateIP)}},
         {"etl_sources.[].ws_port", Array{ConfigValue{ConfigType::String}.optional().withConstraint(validatePort)}},
         {"etl_sources.[].grpc_port", Array{ConfigValue{ConfigType::String}.optional()}},
         {"num_markers", ConfigValue{ConfigType::Integer}.optional()}}
    };

    auto const errors = config.parse(ConfigFileJson{jsonValues});
    ASSERT(!errors.has_value(), "Error parsing Json for clio config for load balancer test");
    return config;
}

// config used for settings provider test
inline ClioConfigDefinition
getParseSettingsConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val};
    auto config = ClioConfigDefinition{
        {"database.cassandra.threads",
         ConfigValue{ConfigType::Integer}.defaultValue(std::thread::hardware_concurrency())},
        {"database.cassandra.contact_points", ConfigValue{ConfigType::String}.defaultValue("127.0.0.1")},
        {"database.cassandra.max_write_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(10000)},
        {"database.cassandra.max_read_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(100000)},
        {"database.cassandra.core_connections_per_host", ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"database.cassandra.certificate", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.username", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.password", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.write_batch_size", ConfigValue{ConfigType::Integer}.defaultValue(20)},
        {"database.cassandra.connect_timeout", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.certfile", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.request_timeout", ConfigValue{ConfigType::Integer}.defaultValue(0)},
        {"database.cassandra.secure_connect_bundle", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.username", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.password", ConfigValue{ConfigType::String}.optional()},
        {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue("clio")},
        {"database.cassandra.port", ConfigValue{ConfigType::Integer}.optional()},
        {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(3)},
        {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.optional()},
    };
    auto const errors = config.parse(jsonVal);
    ASSERT(!errors.has_value(), "Error generating clio config for settings test");
    return config;
};

// config used for cache tests
inline ClioConfigDefinition
generateDefaultCacheConfig()
{
    return ClioConfigDefinition{
        {{"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2)},
         {"cache.num_diffs", ConfigValue{ConfigType::Integer}.defaultValue(32)},
         {"cache.num_markers", ConfigValue{ConfigType::Integer}.defaultValue(48)},
         {"cache.num_cursors_from_diff", ConfigValue{ConfigType::Integer}.defaultValue(0)},
         {"cache.num_cursors_from_account", ConfigValue{ConfigType::Integer}.defaultValue(0)},
         {"cache.page_fetch_size", ConfigValue{ConfigType::Integer}.defaultValue(512)},
         {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async")}}
    };
}

inline ClioConfigDefinition
getParseCacheConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val};
    auto config = generateDefaultCacheConfig();
    auto const errors = config.parse(jsonVal);
    ASSERT(!errors.has_value(), "Error parsing Json for clio config for settings test");
    return config;
}

// config used for server tests
inline ClioConfigDefinition
getParseServerConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val};
    auto config = ClioConfigDefinition{
        {"server.ip", ConfigValue{ConfigType::String}},
        {"server.port", ConfigValue{ConfigType::Integer}},
        {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
        {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
        {"dos_guard.max_fetches", ConfigValue{ConfigType::Integer}},
        {"dos_guard.sweep_interval", ConfigValue{ConfigType::Integer}},
        {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}},
        {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}},
        {"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}.optional()}},
        {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
        {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
    };
    auto const errors = config.parse(jsonVal);
    ASSERT(!errors.has_value(), "Cannot parse Json Correctly");
    return config;
};

inline ClioConfigDefinition
getParseAdminServerConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val};
    auto config = ClioConfigDefinition{
        {"server.ip", ConfigValue{ConfigType::String}},
        {"server.port", ConfigValue{ConfigType::Integer}},
        {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
        {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
        {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
        {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
        {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")}
    };
    auto const errors = config.parse(jsonVal);
    ASSERT(!errors.has_value(), "Cannot parse Server Json Correctly");
    return config;
};

// used for WhitelistHandler tests
inline ClioConfigDefinition
getParseWhitelistHandlerConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val};
    auto config = ClioConfigDefinition{{"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}}}};
    auto errors = config.parse(jsonVal);
    ASSERT(!errors.has_value(), "Cannot parse Json Correctly");
    return config;
}
