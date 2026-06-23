#pragma once

#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "util/config/ObjectView.hpp"

#include <memory>

namespace migration::cassandra {

/**
 * @brief Backfill migrator that indexes historical MPT issuance transactions.
 *
 * Full-scans the transactions table and reuses the live ETL extractor to populate the
 * mptoken_issuance_transactions and account_mptoken_issuance_transactions index tables, letting a
 * node serve complete results for transactions that predate live MPT indexing. The migrator is
 * non-blocking: it deliberately omits kCanBlockClio so a missing backfill never blocks Clio
 * startup.
 */
struct MPTTransactionHistoryMigrator {
    /** @brief Unique name used to identify and invoke this migrator. */
    static constexpr char const* kName = "MPTTransactionHistoryMigrator";

    /** @brief Human-readable description of the migrator. */
    static constexpr char const* kDescription =
        "Backfills the MPT issuance transaction index tables from historical transactions";

    /** @brief The backend type this migrator runs against. */
    using Backend = CassandraMigrationBackend;

    /**
     * @brief Run the backfill: full-scan the transactions table and write MPT index rows.
     *
     * @param backend The migration backend
     * @param config The migration configuration (the .migration config session)
     */
    static void
    runMigration(std::shared_ptr<Backend> const& backend, util::config::ObjectView const& config);
};

}  // namespace migration::cassandra
