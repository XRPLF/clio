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

<<<<<<< HEAD
#include "util/newconfig/ConfigFileInterface.hpp"
#include "util/newconfig/Errors.hpp"
#include "util/newconfig/Object.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace util::config {

=======
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigFileInterface.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Errors.hpp"
#include "util/newconfig/Object.hpp"

#include <optional>
#include <string>

namespace util::config {

/**
 * @brief Full Clio Configuration definition. Specifies which keys it accepts and which fields are mandatory
 */
constexpr auto ClioConfigDefinition = Array{
    Object{
        "Cassandra",
        Object{"contact_points", ConfigValue<std::string>(true)},
        Object{"port", ConfigValue<int>(true)},
        Object{"keyspace", ConfigValue<std::string>(true)},
        Object{"replication_factor", ConfigValue<int>(true)},
        Object{"table_prefix", ConfigValue<std::string>(false)},
        Object{"max_write_requests_outstanding", ConfigValue<std::string>(false)},
        Object{"max_read_requests_outstanding", ConfigValue<std::string>(false)},
        Object{"threads", ConfigValue<std::string>(false)},
        Object{"core_connections_per_host", ConfigValue<std::string>(false)}
    },
    Object{
        "etl_sources",
        Array{
            Object{"ip", ConfigValue<std::string>(true)},
            Object{"ws_port", ConfigValue<std::string>(true)},
            Object{"grpc_port", ConfigValue<std::string>(true)}
        }
    },
    Object{
        "forwarding_cache_timeout",
        Object{"contact_points", ConfigValue<int>(true)},
    },
    Object{"dos_guard", Object{"whitelist", Array{ConfigValue<std::string>(false)}}},
    Object{
        "cache",
        Object{
            "peers",
            Array{Object{"ip", ConfigValue<std::string>(true)}, Object{"port", ConfigValue<std::string>(true)}}
        }
    },
    Object{
        "server",
        Object{"ip", ConfigValue<std::string>(true)},
        Object{"port", ConfigValue<int>(true)},
        Object{"max_queue_size", ConfigValue<int>(false)},
        Object{"local_admin", ConfigValue<bool>(true)},
    },
    Object{"prometheus", Object{"compress_reply", ConfigValue<bool>(true)}},
    Object{
        "log_channels",
        Array{
            Object{"channel", ConfigValue<std::string>(false)},
            Object{"log_level", ConfigValue<std::string>(false)},
        }
    },
    Object{"log_level", ConfigValue<std::string>(false)},
    Object{"log_format", ConfigValue<std::string>(false)},
    Object{"log_to_console", ConfigValue<std::string>(false)},
    Object{"log_directory", ConfigValue<std::string>(false)},
    Object{"log_rotation_size", ConfigValue<std::string>(false)},
    Object{"log_directory_max_size", ConfigValue<std::string>(false)},
    Object{"log_rotation_hour_interval", ConfigValue<std::string>(false)},
    Object{"log_tag_style", ConfigValue<std::string>(false)},
    Object{"extractor_threads", ConfigValue<std::string>(false)},
    Object{"read_only", ConfigValue<std::string>(false)},
    Object{"start_sequence", ConfigValue<std::string>(false)},
    Object{"finish_sequence", ConfigValue<std::string>(false)},
    Object{"ssl_cert_file", ConfigValue<std::string>(false)},
    Object{"ssl_key_file", ConfigValue<std::string>(false)},
    Object{
        "api_version",
        Object{"min", ConfigValue<std::string>(false)},
        Object{"max", ConfigValue<std::string>(false)}
    }
};

>>>>>>> e62e648 (first draft of config)
/** @brief All the config data will be stored and extracted from here.
 *
 * Represents all the data of config
 */
class ConfigFileDefinition {
public:
    ConfigFileDefinition() = default;
    std::optional<Error>
    parse(ConfigFileInterface const& config);
    std::optional<Error>
    validate(ConfigFileInterface const& config) const;
};
<<<<<<< HEAD
extern Object ClioConfig;

struct ClioConfigDescription {
public:
    constexpr std::string_view
    get(std::string_view key) const
    {
        auto const itr = std::find_if(a.begin(), a.end(), [&key](auto const& v) { return v.key == key; });
        if (itr != a.end()) {
            return itr->value;
        }
        return "Not Found";
    }

    struct KV {
        std::string_view key;
        std::string_view value;
    };
    std::array<KV, 4> a;
};
extern ClioConfigDescription const DESCRIPTIONS;
=======
>>>>>>> e62e648 (first draft of config)

}  // namespace util::config
