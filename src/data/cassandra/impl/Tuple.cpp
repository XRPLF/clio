#include "data/cassandra/impl/Tuple.hpp"

#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

namespace {
constexpr auto kTupleDeleter = [](CassTuple* ptr) { cass_tuple_free(ptr); };
constexpr auto kTupleIteratorDeleter = [](CassIterator* ptr) { cass_iterator_free(ptr); };
}  // namespace

namespace data::cassandra::impl {

/* implicit */ Tuple::Tuple(CassTuple* ptr) : ManagedObject{ptr, kTupleDeleter}
{
}

/* implicit */ TupleIterator::TupleIterator(CassIterator* ptr)
    : ManagedObject{ptr, kTupleIteratorDeleter}
{
}

[[nodiscard]] TupleIterator
TupleIterator::fromTuple(CassValue const* value)
{
    return {cass_iterator_from_tuple(value)};
}

}  // namespace data::cassandra::impl
