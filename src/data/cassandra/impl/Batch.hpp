#pragma once

#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

#include <vector>

namespace data::cassandra::impl {

struct Batch : public ManagedObject<CassBatch> {
    Batch(std::vector<Statement> const& statements);

    MaybeError
    add(Statement const& statement);
};

}  // namespace data::cassandra::impl
