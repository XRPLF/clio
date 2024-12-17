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

#include "migration/cassandra/ExampleLedgerMigrator.hpp"

#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "util/Assert.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>

#include <memory>

void
ExampleLedgerMigrator::runMigration(std::shared_ptr<Backend> const& backend, util::config::ObjectView const&)
{
    auto const range =
        data::synchronous([&](boost::asio::yield_context yield) { return backend->hardFetchLedgerRange(yield); });

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
