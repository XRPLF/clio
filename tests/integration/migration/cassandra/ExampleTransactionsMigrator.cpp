#include "migration/cassandra/ExampleTransactionsMigrator.hpp"

#include "data/DBHelpers.hpp"
#include "migration/cassandra/impl/TransactionsAdapter.hpp"
#include "migration/cassandra/impl/Types.hpp"
#include "util/Mutex.hpp"
#include "util/config/ObjectView.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

std::uint64_t ExampleTransactionsMigrator::count;

void
ExampleTransactionsMigrator::runMigration(
    std::shared_ptr<Backend> const& backend,
    util::config::ObjectView const& config
)
{
    auto const ctxFullScanThreads = config.get<std::uint32_t>("full_scan_threads");
    auto const jobsFullScan = config.get<std::uint32_t>("full_scan_jobs");
    auto const cursorPerJobsFullScan = config.get<std::uint32_t>("cursors_per_job");

    using HashSet = std::unordered_set<std::string>;
    util::Mutex<HashSet> hashSet;
    migration::cassandra::impl::TransactionsScanner scanner(
        {.ctxThreadsNum = ctxFullScanThreads,
         .jobsNum = jobsFullScan,
         .cursorsPerJob = cursorPerJobsFullScan},
        migration::cassandra::impl::TransactionsAdapter(
            backend, [&](xrpl::STTx const& tx, xrpl::TxMeta const&) {
                hashSet.lock()->insert(xrpl::to_string(tx.getTransactionID()));
                auto const json = tx.getJson(xrpl::JsonOptions::Values::None);
                auto const txType = json["TransactionType"].asString();
                backend->writeTxIndexExample(uint256ToString(tx.getTransactionID()), txType);
            }
        )
    );
    scanner.wait();
    count = hashSet.lock()->size();
}
