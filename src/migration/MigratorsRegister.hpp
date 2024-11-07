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

#include "data/BackendInterface.hpp"
#include "migration/MigrationManagerInterface.hpp"
#include "migration/Spec.hpp"
#include "util/Concepts.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"

#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace migration {

/**
 *@brief The register of migrators. It will dispatch the migration and rollback to the corresponding migrator. It also
 *hold the shared pointer of backend, which is used by the migrators.
 *
 *@tparam Backend The backend type
 *@tparam MigratorType The migrator types. It should be a concept of MigratorSpec and not have duplicate names.
 */
template <typename Backend, typename... MigratorType>
    requires AllMigratorSpec<MigratorType...>
class MigratorsRegister {
    static_assert(util::hasNoDuplicateNames<MigratorType...>());

    util::Logger log_{"Migration"};
    std::shared_ptr<Backend> backend_;

    template <typename Migrator>
    void
    callMigration(std::string const& name, util::Config const& config)
    {
        if (name == Migrator::name) {
            LOG(log_.info()) << "Running migration: " << name;
            Migrator::runMigration(backend_, config);
            backend_->writeMigratedMigrator(name);
            LOG(log_.info()) << "Finished migration: " << name;
        }
    }

    template <typename Migrator>
    void
    callRollback(std::string const& name)
    {
        if (name == Migrator::name) {
            if constexpr (not RollbackableMigratorSpec<Migrator, Backend>) {
                LOG(log_.warn()) << name << " is not rollbackable, will mark it as not migrated instead";
                std::cout << name
                          << " is not rollbackable, will mark it as not migrated instead, run migration again if needed"
                          << std::endl;
            } else {
                LOG(log_.info()) << "Running migration rollback: " << name;
                Migrator::runRollback(backend_);
            }
            backend_->removeMigratedMigrator(name);
            LOG(log_.info()) << "Finished migration rollback: " << name;
        }
    }

public:
    /**
     * @brief The backend type which is used by the migrators
     */
    using BackendType = Backend;

    /**
     * @brief Construct a new Migrators Register object
     *
     * @param backend The backend shared pointer
     */
    MigratorsRegister(std::shared_ptr<Backend> backend) : backend_{std::move(backend)}
    {
    }

    /**
     * @brief Run the migration according to the given migrator's name
     *
     * @param name The migrator's name
     * @param config The configuration of the migration
     */
    void
    runMigrator(std::string const& name, util::Config const& config)
    {
        (callMigration<MigratorType>(name, config), ...);
    }

    /**
     * @brief Rollback the migration according to the given migrator's name
     *
     * @param name The migrator's name
     */
    void
    runRollback(std::string const& name)
    {
        (callRollback<MigratorType>(name), ...);
    }

    /**
     * @brief Get the status of all the migrators
     *
     * @return A vector of tuple, the first element is the migrator's name, the second element is the status of the
     * migrator
     */
    std::vector<std::tuple<std::string, MigratorStatus>>
    getMigratorsStatus() const
    {
        auto const migratedList = data::synchronous([&](auto yield) { return backend_->fetchMigratedFeatures(yield); });
        auto const fullList = getMigratorNames();

        std::vector<std::tuple<std::string, MigratorStatus>> status;

        for (auto const i : fullList) {
            if (migratedList != std::nullopt &&
                std::find(migratedList->begin(), migratedList->end(), i) != migratedList->end()) {
                status.emplace_back(i, MigratorStatus::Migrated);
            } else {
                status.emplace_back(i, MigratorStatus::NotMigrated);
            }
        }
        return status;
    }

    /**
     * @brief Get the status of a migrator by its name
     *
     * @param name The migrator's name
     * @return The status of the migrator
     */
    MigratorStatus
    getMigratorStatus(std::string const& name) const
    {
        auto const migratedList = data::synchronous([&](auto yield) { return backend_->fetchMigratedFeatures(yield); });
        auto const fullList = getMigratorNames();

        if (std::find(fullList.begin(), fullList.end(), name) == fullList.end())
            return MigratorStatus::NotKnown;

        if (migratedList != std::nullopt &&
            std::find(migratedList->begin(), migratedList->end(), name) != migratedList->end())
            return MigratorStatus::Migrated;

        return MigratorStatus::NotMigrated;
    }

    /**
     * @brief Get all registered migrators' names
     *
     * @return A array of migrator's names
     */
    constexpr auto
    getMigratorNames() const
    {
        return std::array<std::string_view, sizeof...(MigratorType)>{MigratorType::name...};
    }
};

}  // namespace migration
