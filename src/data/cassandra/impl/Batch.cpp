//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "data/cassandra/impl/Batch.hpp"

#include "data/cassandra/Error.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/ManagedObject.hpp"
#include "data/cassandra/impl/Statement.hpp"

#include <cassandra.h>

#include <expected>
#include <stdexcept>
#include <vector>

namespace {
constexpr auto kBATCH_DELETER = [](CassBatch* ptr) { cass_batch_free(ptr); };
}  // namespace

namespace data::cassandra::impl {

/*
 * There are 2 main batches of Cassandra Statements:
 * LOGGED: Ensures all updates in the batch succeed together, or none do.
 * Use this for critical, related changes (e.g., for the same user), but it is slower.
 *
 * UNLOGGED: For performance. Sends many separate updates in one network trip to be fast.
 * Use this for bulk-loading unrelated data, but know there's NO all-or-nothing guarantee.
 *
 * More info here:
 * https://docs.datastax.com/en/developer/cpp-driver-dse/1.10/features/basics/batches/index.html
 */
Batch::Batch(std::vector<Statement> const& statements)
    : ManagedObject{cass_batch_new(CASS_BATCH_TYPE_UNLOGGED), kBATCH_DELETER}
{
    cass_batch_set_is_idempotent(*this, cass_true);

    for (auto const& statement : statements) {
        if (auto const res = add(statement); not res)
            throw std::runtime_error("Failed to add statement to batch: " + res.error());
    }
}

MaybeError
Batch::add(Statement const& statement)
{
    if (auto const rc = cass_batch_add_statement(*this, statement); rc != CASS_OK) {
        return Error{CassandraError{cass_error_desc(rc), rc}};
    }
    return {};
}

}  // namespace data::cassandra::impl
