#pragma once

#include "data/BackendInterface.hpp"
#include "migration/MigrationInspectorInterface.hpp"
#include "migration/MigratiorStatus.hpp"
#include "migration/cassandra/CassandraMigrationManager.hpp"
#include "util/Assert.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <memory>
#include <utility>

namespace migration {

/**
 * @brief A factory function that creates migration inspector instance and initializes the migration
 * table if needed.
 *
 * @param config The config.
 * @param backend The backend instance. It should be initialized before calling this function.
 * @return A shared_ptr<MigrationInspectorInterface> instance
 */
inline std::shared_ptr<MigrationInspectorInterface>
makeMigrationInspector(
    util::config::ClioConfigDefinition const& config,
    std::shared_ptr<BackendInterface> const& backend
)
{
    ASSERT(backend != nullptr, "Backend is not initialized");

    auto inspector = std::make_shared<migration::cassandra::CassandraMigrationInspector>(backend);

    // Database is empty, we need to initialize the migration table if it is a writeable backend
    if (not config.get<bool>("read_only") and not backend->hardFetchLedgerRangeNoThrow()) {
        migration::MigratorStatus const migrated(migration::MigratorStatus::Status::Migrated);
        for (auto const& name : inspector->allMigratorsNames()) {
            backend->writeMigratorStatus(name, migrated.toString());
        }
    }
    return inspector;
}

}  // namespace migration
