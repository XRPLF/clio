#pragma once

#include "data/cassandra/Error.hpp"
#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>
#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <vector>

namespace data::cassandra::impl {

class Collection : public ManagedObject<CassCollection> {
    static constexpr auto kDeleter = [](CassCollection* ptr) { cass_collection_free(ptr); };

public:
    /* implicit */ Collection(CassCollection* ptr);

    template <typename Type>
    explicit Collection(std::vector<Type> const& value)
        : ManagedObject{cass_collection_new(CASS_COLLECTION_TYPE_LIST, value.size()), kDeleter}
    {
        bind(value);
    }

    template <typename Type>
    void
    bind(std::vector<Type> const& values) const
    {
        for (auto const& value : values)
            append(value);
    }

    void
    append(bool const value) const
    {
        auto const rc = cass_collection_append_bool(*this, value ? cass_true : cass_false);
        throwErrorIfNeeded(rc, "Bind bool");
    }

    void
    append(int64_t const value) const
    {
        auto const rc = cass_collection_append_int64(*this, value);
        throwErrorIfNeeded(rc, "Bind int64");
    }

    void
    append(xrpl::uint256 const& value) const
    {
        auto const rc = cass_collection_append_bytes(
            *this,
            static_cast<cass_byte_t const*>(static_cast<unsigned char const*>(value.data())),
            xrpl::uint256::size()
        );
        throwErrorIfNeeded(rc, "Bind xrpl::uint256");
    }
};
}  // namespace data::cassandra::impl
