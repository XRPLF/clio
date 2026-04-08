#pragma once

#include "migration/MigrationManagerInterface.hpp"
#include "migration/impl/MigrationInspectorBase.hpp"
#include "util/config/ObjectView.hpp"

#include <memory>
#include <string>
#include <utility>

namespace migration::impl {

/**
 * @brief The migration manager implementation for Cassandra. It will run the migration for the
 * Cassandra database.
 *
 * @tparam SupportedMigrators The migrators register that contains all the migrators
 */
template <typename SupportedMigrators>
class MigrationManagerBase : public MigrationManagerInterface,
                             public MigrationInspectorBase<SupportedMigrators> {
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
        : MigrationInspectorBase<SupportedMigrators>{std::move(backend)}, config_{std::move(config)}
    {
    }

    /**
     * @brief Run the migration according to the given migrator's name
     *
     * @param name The name of the migrator
     */
    void
    runMigration(std::string const& name) override
    {
        this->migrators_.runMigrator(name, config_);
    }
};

}  // namespace migration::impl
