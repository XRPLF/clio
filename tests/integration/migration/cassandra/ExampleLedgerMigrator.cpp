#include "migration/cassandra/ExampleLedgerMigrator.hpp"

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "util/Assert.hpp"
#include "util/config/ObjectView.hpp"

#include <boost/asio/spawn.hpp>

#include <memory>

void
ExampleLedgerMigrator::runMigration(
    std::shared_ptr<Backend> const& backend,
    util::config::ObjectView const&
)
{
    auto const range = data::synchronous([&](boost::asio::yield_context yield) {
        return backend->hardFetchLedgerRange(yield);
    });

    if (!range.has_value())
        return;

    data::synchronous([&](boost::asio::yield_context yield) {
        for (auto seq = range->minSequence; seq <= range->maxSequence; seq++) {
            auto const ledgerHeader = backend->fetchLedgerBySequence(seq, yield);
            ASSERT(ledgerHeader.has_value(), "Can not find the sequence: {}", seq);

            backend->writeLedgerAccountHash(seq, uint256ToString(ledgerHeader->accountHash));
        }
    });
}
