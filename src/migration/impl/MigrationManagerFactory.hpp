#pragma once

#include "data/LedgerCacheInterface.hpp"
#include "migration/MigrationManagerInterface.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <expected>
#include <memory>
#include <string>

namespace migration::impl {

/**
 * @brief The factory to create a MigrationManagerInterface
 *
 * @param config The configuration of the migration application, it contains the database connection
 * configuration and other migration specific configurations
 * @param cache The ledger cache to use
 * @return A shared pointer to the MigrationManagerInterface if the creation was successful,
 * otherwise an error message
 */
std::expected<std::shared_ptr<MigrationManagerInterface>, std::string>
makeMigrationManager(
    util::config::ClioConfigDefinition const& config,
    data::LedgerCacheInterface& cache
);

}  // namespace migration::impl
