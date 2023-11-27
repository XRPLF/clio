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

#include "data/cassandra/Types.h"
#include "data/cassandra/impl/ManagedObject.h"
#include <cassandra.h>
#include <data/cassandra/Error.h>
#include <data/cassandra/impl/Batch.h>
#include <data/cassandra/impl/Statement.h>
#include <stdexcept>
#include <util/Expected.h>

#include <vector>

namespace {
constexpr auto batchDeleter = [](CassBatch* ptr) { cass_batch_free(ptr); };
}  // namespace

namespace data::cassandra::detail {

// TODO: Use an appropriate value instead of CASS_BATCH_TYPE_LOGGED for different use cases
Batch::Batch(std::vector<Statement> const& statements)
    : ManagedObject{cass_batch_new(CASS_BATCH_TYPE_LOGGED), batchDeleter}
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

}  // namespace data::cassandra::detail
