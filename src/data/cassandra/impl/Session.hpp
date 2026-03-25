#pragma once

#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

namespace data::cassandra::impl {

class Session : public ManagedObject<CassSession> {
    static constexpr auto kDELETER = [](CassSession* ptr) { cass_session_free(ptr); };

public:
    Session() : ManagedObject{cass_session_new(), kDELETER}
    {
    }
};

}  // namespace data::cassandra::impl
