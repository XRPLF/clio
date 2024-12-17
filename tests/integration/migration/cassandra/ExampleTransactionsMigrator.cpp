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

#include "migration/cassandra/ExampleTransactionsMigrator.hpp"

#include "data/DBHelpers.hpp"
#include "migration/cassandra/impl/TransactionsAdapter.hpp"
#include "migration/cassandra/impl/Types.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <memory>
#include <mutex>
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

    std::unordered_set<std::string> hashSet;
    std::mutex mtx;  // protect hashSet
    migration::cassandra::impl::TransactionsScanner scaner(
        {.ctxThreadsNum = ctxFullScanThreads, .jobsNum = jobsFullScan, .cursorsPerJob = cursorPerJobsFullScan},
        migration::cassandra::impl::TransactionsAdapter(
            backend,
            [&](ripple::STTx const& tx, ripple::TxMeta const&) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    hashSet.insert(ripple::to_string(tx.getTransactionID()));
                }
                auto const json = tx.getJson(ripple::JsonOptions::none);
                auto const txType = json["TransactionType"].asString();
                backend->writeTxIndexExample(uint256ToString(tx.getTransactionID()), txType);
            }
        )
    );
    scaner.wait();
    count = hashSet.size();
}
