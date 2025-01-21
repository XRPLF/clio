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

#include "migration/MigrationApplication.hpp"

#include "migration/MigratiorStatus.hpp"
#include "migration/impl/MigrationManagerFactory.hpp"
#include "util/OverloadSet.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace app {

MigratorApplication::MigratorApplication(util::config::ClioConfigDefinition const& config, MigrateSubCmd command)
    : cmd_(std::move(command))
{
    PrometheusService::init(config);

    auto expectedMigrationManager = migration::impl::makeMigrationManager(config);

    if (not expectedMigrationManager) {
        throw std::runtime_error("Failed to create migration manager: " + expectedMigrationManager.error());
    }

    migrationManager_ = std::move(expectedMigrationManager.value());
}

int
MigratorApplication::run()
{
    return std::visit(
        util::OverloadSet{
            [this](MigrateSubCmd::Status const&) { return printStatus(); },
            [this](MigrateSubCmd::Migration const& cmdBundle) { return migrate(cmdBundle.migratorName); }
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
        std::cout << "Migrator: " << migrator << " - " << migrationManager_->getMigratorDescriptionByName(migrator)
                  << " - " << status.toString() << std::endl;
    }
    return EXIT_SUCCESS;
}

int
MigratorApplication::migrate(std::string const& migratorName)
{
    auto const status = migrationManager_->getMigratorStatusByName(migratorName);
    if (status == migration::MigratorStatus::Migrated) {
        std::cout << "Migrator " << migratorName << " has already migrated" << std::endl;
        printStatus();
        return EXIT_SUCCESS;
    }

    if (status == migration::MigratorStatus::NotKnown) {
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
