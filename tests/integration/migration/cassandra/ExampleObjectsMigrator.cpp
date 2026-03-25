#include "migration/cassandra/ExampleObjectsMigrator.hpp"

#include "migration/cassandra/impl/ObjectsAdapter.hpp"
#include "migration/cassandra/impl/Types.hpp"
#include "util/config/ObjectView.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/STLedgerEntry.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>

std::atomic_int64_t ExampleObjectsMigrator::count;
std::atomic_int64_t ExampleObjectsMigrator::accountCount;

void
ExampleObjectsMigrator::runMigration(
    std::shared_ptr<Backend> const& backend,
    util::config::ObjectView const& config
)
{
    auto const ctxFullScanThreads = config.get<std::uint32_t>("full_scan_threads");
    auto const jobsFullScan = config.get<std::uint32_t>("full_scan_jobs");
    auto const cursorPerJobsFullScan = config.get<std::uint32_t>("cursors_per_job");

    std::unordered_set<ripple::uint256> idx;
    migration::cassandra::impl::ObjectsScanner scanner(
        {.ctxThreadsNum = ctxFullScanThreads,
         .jobsNum = jobsFullScan,
         .cursorsPerJob = cursorPerJobsFullScan},
        migration::cassandra::impl::ObjectsAdapter(
            backend, [&](std::uint32_t, std::optional<ripple::SLE> sle) {
                if (sle.has_value()) {
                    if (sle->getType() == ripple::ltACCOUNT_ROOT) {
                        if (!idx.contains(sle->key())) {
                            ExampleObjectsMigrator::accountCount++;
                        }
                    }
                    idx.insert(sle->key());
                    ExampleObjectsMigrator::count++;
                }
            }
        )
    );
    scanner.wait();
}
