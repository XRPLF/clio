#pragma once

#include "migration/cassandra/CassandraMigrationTestBackend.hpp"
#include "util/config/ObjectView.hpp"

#include <cstdint>
#include <memory>

/**
 * @brief Example migrator for the transactions table. In this example, we show how to traverse the
 * transactions table and migrate the data to index table. We create an index table for transaction
 * hash to transaction type string.
 */
struct ExampleTransactionsMigrator {
    static constexpr char const* kName = "ExampleTransactionsMigrator";
    static constexpr char const* kDescription = "The migrator for transactions table";

    using Backend = CassandraMigrationTestBackend;
    static std::uint64_t count;

    static void
    runMigration(std::shared_ptr<Backend> const& backend, util::config::ObjectView const& config);
};
