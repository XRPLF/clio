#include "data/cassandra/impl/Tuple.hpp"

#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

namespace {
constexpr auto kTUPLE_DELETER = [](CassTuple* ptr) { cass_tuple_free(ptr); };
constexpr auto kTUPLE_ITERATOR_DELETER = [](CassIterator* ptr) { cass_iterator_free(ptr); };
}  // namespace

namespace data::cassandra::impl {

/* implicit */ Tuple::Tuple(CassTuple* ptr) : ManagedObject{ptr, kTUPLE_DELETER}
{
}

/* implicit */ TupleIterator::TupleIterator(CassIterator* ptr)
    : ManagedObject{ptr, kTUPLE_ITERATOR_DELETER}
{
}

[[nodiscard]] TupleIterator
TupleIterator::fromTuple(CassValue const* value)
{
    return {cass_iterator_from_tuple(value)};
}

}  // namespace data::cassandra::impl
