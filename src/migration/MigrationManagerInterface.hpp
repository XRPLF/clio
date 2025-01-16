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

#include "migration/MigratiorStatus.hpp"

#include <string>
#include <tuple>
#include <vector>

namespace migration {

/**
 * @brief The interface for the migration manager. This interface is tend to be implemented for specific database. The
 * application layer will use this interface to run the migrations.
 */
struct MigrationManagerInterface {
    virtual ~MigrationManagerInterface() = default;

    /**
     * @brief Run the the migration according to the given migrator's name
     */
    virtual void
    runMigration(std::string const&) = 0;

    /**
     * @brief Get the status of all the migrators
     * @return A vector of tuple, the first element is the migrator's name, the second element is the status of the
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
};

}  // namespace migration
