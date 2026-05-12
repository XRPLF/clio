#include "migration/MigrationApplication.hpp"

#include "migration/MigratiorStatus.hpp"
#include "migration/impl/MigrationManagerFactory.hpp"
#include "util/OverloadSet.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace app {

MigratorApplication::MigratorApplication(
    util::config::ClioConfigDefinition const& config,
    MigrateSubCmd command
)
    : cmd_(std::move(command))
{
    PrometheusService::init(config);

    auto expectedMigrationManager = migration::impl::makeMigrationManager(config, cache_);

    if (not expectedMigrationManager) {
        throw std::runtime_error(
            "Failed to create migration manager: " + expectedMigrationManager.error()
        );
    }

    migrationManager_ = std::move(expectedMigrationManager.value());
}

int
MigratorApplication::run()
{
    return std::visit(
        util::OverloadSet{
            [this](MigrateSubCmd::Status const&) { return printStatus(); },
            [this](MigrateSubCmd::Migration const& cmdBundle) {
                return migrate(cmdBundle.migratorName);
            }
        },
        cmd_.state
    );
}

int
MigratorApplication::printStatus()
{
    std::cout << "Current Migration Status:" << std::endl;
    auto const allMigratorsStatusPairs = migrationManager_->allMigratorsStatusPairs();

    if (allMigratorsStatusPairs.empty()) {
        std::cout << "No migrator found" << std::endl;
    }

    for (auto const& [migrator, status] : allMigratorsStatusPairs) {
        std::cout << "Migrator: " << migrator << " - "
                  << migrationManager_->getMigratorDescriptionByName(migrator) << " - "
                  << status.toString() << std::endl;
    }
    return EXIT_SUCCESS;
}

int
MigratorApplication::migrate(std::string const& migratorName)
{
    auto const status = migrationManager_->getMigratorStatusByName(migratorName);
    if (status == migration::MigratorStatus::Status::Migrated) {
        std::cout << "Migrator " << migratorName << " has already migrated" << std::endl;
        printStatus();
        return EXIT_SUCCESS;
    }

    if (status == migration::MigratorStatus::Status::NotKnown) {
        std::cout << "Migrator " << migratorName << " not found" << std::endl;
        printStatus();
        return EXIT_FAILURE;
    }

    std::cout << "Running migration for " << migratorName << std::endl;
    migrationManager_->runMigration(migratorName);
    std::cout << "Migration for " << migratorName << " has finished" << std::endl;
    return EXIT_SUCCESS;
}

}  // namespace app
