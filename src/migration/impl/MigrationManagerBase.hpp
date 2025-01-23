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

#include "migration/MigrationManagerInterface.hpp"
#include "migration/impl/MigrationInspectorBase.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <memory>
#include <string>
#include <utility>

namespace migration::impl {

/**
 * @brief The migration manager implementation for Cassandra. It will run the migration for the Cassandra
 * database.
 *
 * @tparam SupportedMigrators The migrators resgister that contains all the migrators
 */
template <typename SupportedMigrators>
class MigrationManagerBase : public MigrationManagerInterface, public MigrationInspectorBase<SupportedMigrators> {
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
     * @brief Run the the migration according to the given migrator's name
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
