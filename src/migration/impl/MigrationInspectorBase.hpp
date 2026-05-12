#pragma once

#include "migration/MigrationInspectorInterface.hpp"
#include "migration/MigratiorStatus.hpp"

#include <memory>
#include <ranges>
#include <string>
#include <tuple>
#include <vector>

namespace migration::impl {

/**
 * @brief The migration inspector implementation for Cassandra. It will report the migration status
 * for Cassandra database.
 *
 * @tparam SupportedMigrators The migrators register that contains all the migrators
 */
template <typename SupportedMigrators>
class MigrationInspectorBase : virtual public MigrationInspectorInterface {
protected:
    SupportedMigrators migrators_;

public:
    /**
     * @brief Construct a new Cassandra Migration Inspector object
     *
     * @param backend The backend of the Cassandra database
     */
    explicit MigrationInspectorBase(
        std::shared_ptr<typename SupportedMigrators::BackendType> backend
    )
        : migrators_{std::move(backend)}
    {
    }

    /**
     * @brief Get the status of all the migrators
     *
     * @return A vector of tuple, the first element is the migrator's name, the second element is
     * the status of the migrator
     */
    [[nodiscard]] std::vector<std::tuple<std::string, MigratorStatus>>
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
    [[nodiscard]] MigratorStatus
    getMigratorStatusByName(std::string const& name) const override
    {
        return migrators_.getMigratorStatus(name);
    }

    /**
     * @brief Get all registered migrators' names
     *
     * @return A vector of string, the names of all the migrators
     */
    [[nodiscard]] std::vector<std::string>
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
    [[nodiscard]] std::string
    getMigratorDescriptionByName(std::string const& name) const override
    {
        return migrators_.getMigratorDescription(name);
    }

    /**
     * @brief Return if there is incomplete migrator blocking the server
     *
     * @return True if server is blocked, false otherwise
     */
    [[nodiscard]] bool
    isBlockingClio() const override
    {
        return std::ranges::any_of(migrators_.getMigratorNames(), [&](auto const& migrator) {
            if (auto canBlock = migrators_.canMigratorBlockClio(migrator); canBlock.has_value() and
                *canBlock and
                migrators_.getMigratorStatus(std::string(migrator)) ==
                    MigratorStatus::Status::NotMigrated) {
                return true;
            }
            return false;
        });
    }
};

}  // namespace migration::impl
