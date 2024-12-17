//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022-2024, the clio developers.

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

#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Schema.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "data/cassandra/Types.hpp"

#include <fmt/core.h>

#include <functional>
#include <string>

namespace migration::cassandra::impl {

/**
 * @brief The schema for the migration process. It contains the prepared statements only used for the migration process.
 */
class CassandraMigrationSchema {
    using SettingsProviderType = data::cassandra::SettingsProvider;
    std::reference_wrapper<SettingsProviderType const> settingsProvider_;

public:
    /**
     * @brief Construct a new Cassandra Migration Schema object
     *
     * @param settings The settings provider of database
     */
    explicit CassandraMigrationSchema(SettingsProviderType const& settings) : settingsProvider_{settings}
    {
    }

    /**
     * @brief Get the prepared statement for the full scan of a table
     *
     * @param handler The database handler
     * @param tableName The name of the table
     * @param key The partition key of the table
     * @return The prepared statement
     */
    data::cassandra::PreparedStatement
    getPreparedFullScanStatement(
        data::cassandra::Handle const& handler,
        std::string const& tableName,
        std::string const& key
    )
    {
        return handler.prepare(fmt::format(
            R"(
            SELECT * 
              FROM {} 
             WHERE TOKEN({}) >= ? AND TOKEN({}) <= ?
            )",
            data::cassandra::qualifiedTableName<SettingsProviderType>(settingsProvider_.get(), tableName),
            key,
            key
        ));
    }

    /**
     * @brief Get the prepared statement for insertion of migrator_status table
     *
     * @param handler The database handler
     * @return The prepared statement to insert into migrator_status table
     */
    data::cassandra::PreparedStatement const&
    getPreparedInsertMigratedMigrator(data::cassandra::Handle const& handler)
    {
        static auto prepared = handler.prepare(fmt::format(
            R"(
            INSERT INTO {} 
                   (migrator_name, status)
            VALUES (?, ?)
            )",
            data::cassandra::qualifiedTableName<SettingsProviderType>(settingsProvider_.get(), "migrator_status")
        ));
        return prepared;
    }
};
}  // namespace migration::cassandra::impl
