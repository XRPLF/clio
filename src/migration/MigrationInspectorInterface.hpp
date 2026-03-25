#pragma once

#include "migration/MigratiorStatus.hpp"

#include <string>
#include <tuple>
#include <vector>

namespace migration {

/**
 * @brief The interface for the migration inspector.The Clio server application will use this
 * interface to inspect the migration status.
 */
struct MigrationInspectorInterface {
    virtual ~MigrationInspectorInterface() = default;

    /**
     * @brief Get the status of all the migrators
     * @return A vector of tuple, the first element is the migrator's name, the second element is
     * the status of the
     */
    virtual std::vector<std::tuple<std::string, MigratorStatus>>
    allMigratorsStatusPairs() const = 0;

    /**
     * @brief Get all registered migrators' names
     *
     * @return A vector of migrators' names
     */
    virtual std::vector<std::string>
    allMigratorsNames() const = 0;

    /**
     * @brief Get the status of a migrator by its name
     *
     * @param name The migrator's name
     * @return The status of the migrator
     */
    virtual MigratorStatus
    getMigratorStatusByName(std::string const& name) const = 0;

    /**
     * @brief Get the description of a migrator by its name
     *
     * @param name The migrator's name
     * @return The description of the migrator
     */
    virtual std::string
    getMigratorDescriptionByName(std::string const& name) const = 0;

    /**
     * @brief Return if Clio server is blocked
     *
     * @return True if Clio server is blocked by migration, false otherwise
     */
    virtual bool
    isBlockingClio() const = 0;
};

}  // namespace migration
