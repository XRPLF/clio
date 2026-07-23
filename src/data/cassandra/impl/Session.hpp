#pragma once

#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

namespace data::cassandra::impl {

class Session : public ManagedObject<CassSession> {
    static constexpr auto kDeleter = [](CassSession* ptr) { cass_session_free(ptr); };

public:
    Session() : ManagedObject{cass_session_new(), kDeleter}
    {
    }
};

}  // namespace data::cassandra::impl
