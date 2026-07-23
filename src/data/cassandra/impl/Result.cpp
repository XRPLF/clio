#include "data/cassandra/impl/Result.hpp"

#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

#include <cstddef>

namespace {
constexpr auto kResultDeleter = [](CassResult const* ptr) { cass_result_free(ptr); };
constexpr auto kResultIteratorDeleter = [](CassIterator* ptr) { cass_iterator_free(ptr); };
}  // namespace

namespace data::cassandra::impl {

/* implicit */ Result::Result(CassResult const* ptr) : ManagedObject{ptr, kResultDeleter}
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
    : ManagedObject{ptr, kResultIteratorDeleter}, hasMore_{cass_iterator_next(ptr) != 0u}
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
    hasMore_ = (cass_iterator_next(*this) != 0u);
    return hasMore_;
}

[[nodiscard]] bool
ResultIterator::hasMore() const
{
    return hasMore_;
}

}  // namespace data::cassandra::impl
