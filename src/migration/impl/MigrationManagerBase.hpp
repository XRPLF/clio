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

#include "migration/MigrationManagerInterface.hpp"
#include "migration/MigratiorStatus.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace migration::impl {

/**
 * @brief The migration manager implementation for Cassandra. It will run the migration for the Cassandra
 * database.
 *
 * @tparam SupportedMigrators The migrators resgister that contains all the migrators
 */
template <typename SupportedMigrators>
class MigrationManagerBase : public MigrationManagerInterface {
    SupportedMigrators migrators_;
    // contains only migration related settings
    util::config::ObjectView config_;

public:
    /**
     * @brief Construct a new Cassandra Migration Manager object
     *
     * @param backend The backend of the Cassandra database
     * @param config The configuration of the migration
     */
    explicit MigrationManagerBase(
        std::shared_ptr<typename SupportedMigrators::BackendType> backend,
        util::config::ObjectView config
    )
        : migrators_{backend}, config_{std::move(config)}
    {
    }

    /**
     * @brief Run the the migration according to the given migrator's name
     *
     * @param name The name of the migrator
     */
    void
    runMigration(std::string const& name) override
    {
        migrators_.runMigrator(name, config_);
    }

    /**
     * @brief Get the status of all the migrators
     *
     * @return A vector of tuple, the first element is the migrator's name, the second element is the status of the
     * migrator
     */
    std::vector<std::tuple<std::string, MigratorStatus>>
    allMigratorsStatusPairs() const override
    {
        return migrators_.getMigratorsStatus();
    }

    /**
     * @brief Get the status of a migrator by its name
     *
     * @param name The name of the migrator
     * @return The status of the migrator
     */
    MigratorStatus
    getMigratorStatusByName(std::string const& name) const override
    {
        return migrators_.getMigratorStatus(name);
    }

    /**
     * @brief Get all registered migrators' names
     *
     * @return A vector of string, the names of all the migrators
     */
    std::vector<std::string>
    allMigratorsNames() const override
    {
        auto const names = migrators_.getMigratorNames();
        return std::vector<std::string>{names.begin(), names.end()};
    }

    /**
     * @brief Get the description of a migrator by its name
     *
     * @param name The name of the migrator
     * @return The description of the migrator
     */
    std::string
    getMigratorDescriptionByName(std::string const& name) const override
    {
        return migrators_.getMigratorDescription(name);
    }
};

}  // namespace migration::impl
