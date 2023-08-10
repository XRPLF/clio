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

#include <data/cassandra/impl/Result.h>

namespace {
static constexpr auto resultDeleter = [](CassResult const* ptr) { cass_result_free(ptr); };
static constexpr auto resultIteratorDeleter = [](CassIterator* ptr) { cass_iterator_free(ptr); };
}  // namespace

namespace data::cassandra::detail {

/* implicit */ Result::Result(CassResult const* ptr) : ManagedObject{ptr, resultDeleter}
{
}

[[nodiscard]] std::size_t
Result::numRows() const
{
    return cass_result_row_count(*this);
}

[[nodiscard]] bool
Result::hasRows() const
{
    return numRows() > 0;
}

/* implicit */ ResultIterator::ResultIterator(CassIterator* ptr)
    : ManagedObject{ptr, resultIteratorDeleter}, hasMore_{cass_iterator_next(ptr)}
{
}

[[nodiscard]] ResultIterator
ResultIterator::fromResult(Result const& result)
{
    return {cass_iterator_from_result(result)};
}

[[maybe_unused]] bool
ResultIterator::moveForward()
{
    hasMore_ = cass_iterator_next(*this);
    return hasMore_;
}

[[nodiscard]] bool
ResultIterator::hasMore() const
{
    return hasMore_;
}

}  // namespace data::cassandra::detail
