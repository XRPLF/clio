#pragma once

#include "migration/cassandra/CassandraMigrationTestBackend.hpp"
#include "util/config/ObjectView.hpp"

#include <memory>

/**
 * @brief Example migrator for dropping the table. In this example, our migrator will drop the
 * table. The table removal is not reversible.
 */
struct ExampleDropTableMigrator {
    using Backend = CassandraMigrationTestBackend;

    static constexpr char const* kNAME = "ExampleDropTableMigrator";
    static constexpr char const* kDESCRIPTION = "The migrator for dropping the table";

    static void
    runMigration(std::shared_ptr<Backend> const& backend, util::config::ObjectView const& config);
};
