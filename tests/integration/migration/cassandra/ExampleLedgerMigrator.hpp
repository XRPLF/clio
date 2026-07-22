#pragma once

#include "migration/cassandra/CassandraMigrationTestBackend.hpp"
#include "util/config/ObjectView.hpp"

#include <memory>

/**
 * @brief Example migrator for the ledgers table. In this example, we show how to migrate the data
 * from table without full table scan. We create an index table called "ledger_example" which
 * maintains the map of ledger sequence to account hash. Because ledger sequence is the partition
 * key of ledgers table, we can just fetch the data via ledger sequence without full table scan.
 */
struct ExampleLedgerMigrator {
    static constexpr char const* kName = "ExampleLedgerMigrator";
    static constexpr char const* kDescription = "The migrator for ledgers table";

    using Backend = CassandraMigrationTestBackend;

    static void
    runMigration(std::shared_ptr<Backend> const& backend, util::config::ObjectView const& config);
};
