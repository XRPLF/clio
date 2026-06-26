#include "migration/cassandra/MPTTransactionHistoryMigrator.hpp"

#include "etl/MPTHelpers.hpp"
#include "migration/cassandra/impl/TransactionsAdapter.hpp"
#include "migration/cassandra/impl/Types.hpp"
#include "util/config/ObjectView.hpp"

#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <memory>

namespace migration::cassandra {

void
MPTTransactionHistoryMigrator::runMigration(
    std::shared_ptr<Backend> const& backend,
    util::config::ObjectView const& config
)
{
    auto const fullScanThreads = config.get<std::uint32_t>("full_scan_threads");
    auto const fullScanJobs = config.get<std::uint32_t>("full_scan_jobs");
    auto const cursorsPerJob = config.get<std::uint32_t>("cursors_per_job");

    // Full-scan the transactions table in parallel; for each transaction reuse the live ETL
    // extractor to derive the touched MPT issuances and affected accounts, then write both index
    // shapes. Token-range-edge re-reads and post-crash reruns re-upsert identical deterministic-key
    // rows, so the scan is idempotent without explicit deduplication.
    impl::TransactionsScanner scanner(
        {.ctxThreadsNum = fullScanThreads, .jobsNum = fullScanJobs, .cursorsPerJob = cursorsPerJob},
        impl::TransactionsAdapter(backend, [&](xrpl::STTx const& sttx, xrpl::TxMeta const& txMeta) {
            auto const indexData = etl::getMPTokenIssuanceTxsFromTx(txMeta, sttx);
            if (indexData.empty())
                return;

            backend->writeMPTokenIssuanceTransactions(indexData);
            backend->writeAccountMPTokenIssuanceTransactions(indexData);
        })
    );
    scanner.waitForAllAndThrowOnError();

    // Flush queued async writes so the migrator is not marked Migrated before all index rows are
    // durable.
    backend->waitForWritesToFinish();
}

}  // namespace migration::cassandra
