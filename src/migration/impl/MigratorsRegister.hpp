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

#pragma once

#include "data/BackendInterface.hpp"
#include "migration/MigratiorStatus.hpp"
#include "migration/impl/Spec.hpp"
#include "util/Assert.hpp"
#include "util/Concepts.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace migration::impl {

/**
 * The concept to check if BackendType is the same as the migrator's required backend type
 */
template <typename BackendType, typename MigratorType>
concept MigrationBackend = requires { requires std::same_as<typename MigratorType::Backend, BackendType>; };

template <typename Backend, typename... MigratorType>
concept BackendMatchAllMigrators = (MigrationBackend<Backend, MigratorType> && ...);

template <typename T>
concept HasCanBlockClio = requires(T t) {
    { t.kCAN_BLOCK_CLIO };
};

/**
 *@brief The register of migrators. It will dispatch the migration to the corresponding migrator. It also
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
    callMigration(std::string const& name, util::config::ObjectView const& config)
    {
        if (name == Migrator::kNAME) {
            LOG(log_.info()) << "Running migration: " << name;
            Migrator::runMigration(backend_, config);
            backend_->writeMigratorStatus(name, MigratorStatus(MigratorStatus::Migrated).toString());
            LOG(log_.info()) << "Finished migration: " << name;
        }
    }

    template <typename T>
    static constexpr std::string_view
    getDescriptionIfMatch(std::string_view targetName)
    {
        return (T::kNAME == targetName) ? T::kDESCRIPTION : "";
    }

    template <typename First, typename... Rest>
    static constexpr bool
    canBlockClioHelper(std::string_view targetName)
    {
        if (targetName == First::kNAME) {
            if constexpr (HasCanBlockClio<First>) {
                return First::kCAN_BLOCK_CLIO;
            }
            return false;
        }
        if constexpr (sizeof...(Rest) > 0) {
            return canBlockClioHelper<Rest...>(targetName);
        }
        ASSERT(false, "The migrator name is not found");
        std::unreachable();
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
    MigratorsRegister(std::shared_ptr<BackendType> backend) : backend_{std::move(backend)}
    {
    }

    /**
     * @brief Run the migration according to the given migrator's name
     *
     * @param name The migrator's name
     * @param config The configuration of the migration
     */
    void
    runMigrator(std::string const& name, util::config::ObjectView const& config)
        requires BackendMatchAllMigrators<BackendType, MigratorType...>
    {
        (callMigration<MigratorType>(name, config), ...);
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
        auto const fullList = getMigratorNames();

        std::vector<std::tuple<std::string, MigratorStatus>> status;

        std::ranges::transform(fullList, std::back_inserter(status), [&](auto const& migratorName) {
            auto const migratorNameStr = std::string(migratorName);
            return std::make_tuple(migratorNameStr, getMigratorStatus(migratorNameStr));
        });
        return status;
    }

    /**
     * @brief Get the status of a migrator by its name
     *
     * @param name The migrator's name to get the status
     * @return The status of the migrator
     */
    MigratorStatus
    getMigratorStatus(std::string const& name) const
    {
        auto const fullList = getMigratorNames();
        if (std::ranges::find(fullList, name) == fullList.end()) {
            return MigratorStatus::NotKnown;
        }
        auto const statusStringOpt =
            data::synchronous([&](auto yield) { return backend_->fetchMigratorStatus(name, yield); });

        return statusStringOpt ? MigratorStatus::fromString(statusStringOpt.value()) : MigratorStatus::NotMigrated;
    }

    /**
     * @brief Get all registered migrators' names
     *
     * @return A array of migrator's names
     */
    constexpr auto
    getMigratorNames() const
    {
        return std::array<std::string_view, sizeof...(MigratorType)>{MigratorType::kNAME...};
    }

    /**
     * @brief Get the description of a migrator by its name
     *
     * @param name The migrator's name
     * @return The description of the migrator
     */
    std::string
    getMigratorDescription(std::string const& name) const
    {
        if constexpr (sizeof...(MigratorType) == 0) {
            return "No Description";
        } else {
            // Fold expression to search through all types
            std::string const result = ([](std::string const& name) {
                return std::string(getDescriptionIfMatch<MigratorType>(name));
            }(name) + ...);

            return result.empty() ? "No Description" : result;
        }
    }

    /**
     * @brief Return if the given migrator can block Clio server
     *
     * @param name The migrator's name
     * @return std::nullopt if the migrator name is not found, or a boolean value indicating whether the migrator is
     * blocking Clio server.
     */
    std::optional<bool>
    canMigratorBlockClio(std::string_view name) const
    {
        if constexpr (sizeof...(MigratorType) == 0) {
            return std::nullopt;
        } else {
            auto const migratiors = getMigratorNames();
            if (std::ranges::find(migratiors, name) == migratiors.end())
                return std::nullopt;

            return canBlockClioHelper<MigratorType...>(name);
        }
    }
};

}  // namespace migration::impl
