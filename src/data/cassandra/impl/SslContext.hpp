#pragma once

#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

#include <string>

namespace data::cassandra::impl {

struct SslContext : public ManagedObject<CassSsl> {
    explicit SslContext(std::string const& certificate);
};

}  // namespace data::cassandra::impl
