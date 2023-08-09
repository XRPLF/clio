//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <data/cassandra/Handle.h>
#include <data/cassandra/Types.h>
#include <util/Expected.h>
#include <util/config/Config.h>
#include <util/log/Logger.h>

namespace data::cassandra {

/**
 * @brief Provides settings for @ref CassandraBackend
 */
class SettingsProvider
{
    clio::util::Config config_;

    std::string keyspace_;
    std::optional<std::string> tablePrefix_;
    uint16_t replicationFactor_;
    uint16_t ttl_;
    Settings settings_;

public:
    explicit SettingsProvider(clio::util::Config const& cfg, uint16_t ttl = 0);

    /*! Get the cluster settings */
    [[nodiscard]] Settings
    getSettings() const;

    /*! Get the specified keyspace */
    [[nodiscard]] inline std::string
    getKeyspace() const
    {
        return keyspace_;
    }

    /*! Get an optional table prefix to use in all queries */
    [[nodiscard]] inline std::optional<std::string>
    getTablePrefix() const
    {
        return tablePrefix_;
    }

    /*! Get the replication factor */
    [[nodiscard]] inline uint16_t
    getReplicationFactor() const
    {
        return replicationFactor_;
    }

    /*! Get the default time to live to use in all `create` queries */
    [[nodiscard]] inline uint16_t
    getTtl() const
    {
        return ttl_;
    }

private:
    [[nodiscard]] std::optional<std::string>
    parseOptionalCertificate() const;

    [[nodiscard]] Settings
    parseSettings() const;
};

}  // namespace data::cassandra
