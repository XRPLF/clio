#pragma once

#include "migration/cassandra/CassandraMigrationTestBackend.hpp"
#include "util/config/ObjectView.hpp"

#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>

#include <atomic>
#include <memory>

/**
 * @brief Example migrator for the objects table. In this example, we show how to traverse objects
 * table. We will count the number of account root in the objects table.
 */
struct ExampleObjectsMigrator {
    using Backend = CassandraMigrationTestBackend;

    static constexpr char const* kName = "ExampleObjectsMigrator";
    static constexpr char const* kDescription = "The migrator for objects table";

    static std::atomic_int64_t count;
    static std::atomic_int64_t accountCount;

    static void
    runMigration(std::shared_ptr<Backend> const& backend, util::config::ObjectView const& config);
};
