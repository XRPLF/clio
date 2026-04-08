#pragma once

#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/Cluster.hpp"
#include "util/config/ObjectView.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace data::cassandra {

/**
 * @brief Provides settings for @ref BasicCassandraBackend.
 */
class SettingsProvider {
    util::config::ObjectView config_;

    std::string keyspace_;
    std::optional<std::string> tablePrefix_;
    uint16_t replicationFactor_;
    Settings settings_;

public:
    /**
     * @brief Create a settings provider from the specified config.
     *
     * @param cfg The config of Clio to use
     */
    explicit SettingsProvider(util::config::ObjectView const& cfg);

    /**
     * @return The cluster settings
     */
    [[nodiscard]] Settings
    getSettings() const;

    /**
     * @return The specified keyspace
     */
    [[nodiscard]] std::string
    getKeyspace() const
    {
        return keyspace_;
    }

    /**
     * @return The optional table prefix to use in all queries
     */
    [[nodiscard]] std::optional<std::string>
    getTablePrefix() const
    {
        return tablePrefix_;
    }

    /**
     * @return The replication factor
     */
    [[nodiscard]] uint16_t
    getReplicationFactor() const
    {
        return replicationFactor_;
    }

private:
    [[nodiscard]] std::optional<std::string>
    parseOptionalCertificate() const;

    [[nodiscard]] Settings
    parseSettings() const;
};

}  // namespace data::cassandra
