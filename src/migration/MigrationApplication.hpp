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

#include "migration/MigrationManagerInterface.hpp"
#include "util/config/Config.hpp"

#include <memory>
#include <string>
#include <variant>
namespace app {

/**
 * @brief The migration application class
 */
class MigratorApplication {
    std::string option_;
    std::shared_ptr<migration::MigrationManagerInterface> migrationManager_;

public:
    /**
     * @brief The command to run
     */
    struct Cmd {
        /**
         * @brief Check the status of the migrations
         */
        struct Status {};
        /**
         * @brief Run a migration
         */
        struct Migration {
            std::string migratorName;
        };
        /**
         * @brief Rollback a migration
         */
        struct Rollback {
            std::string migratorName;
        };

        std::variant<Status, Migration, Rollback> state;

        /**
         * @brief Helper function to create a status command
         */
        static Cmd
        status()
        {
            return Cmd{Status{}};
        }

        /**
         * @brief Helper function to create a migration command
         */
        static Cmd
        migration(std::string const& name)
        {
            return Cmd{Migration{name}};
        }

        /**
         * @brief Helper function to create a rollback command
         */
        static Cmd
        rollback(std::string const& name)
        {
            return Cmd{Rollback{name}};
        }
    };

    /**
     * @brief Construct a new MigratorApplication object
     *
     * @param config The configuration of the application
     * @param command The command to run
     */
    MigratorApplication(util::Config const& config, Cmd command);

    /**
     * @brief Run the application
     *
     * @return exit code
     */
    int
    run();

private:
    int
    printStatus();

    int
    migrate(std::string const& name);

    int
    rollback(std::string const& name);

    Cmd cmd_;
};
}  // namespace app
