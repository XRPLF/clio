#pragma once

#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>
#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace data::cassandra::impl {

class Collection : public ManagedObject<CassCollection> {
    static constexpr auto kDELETER = [](CassCollection* ptr) { cass_collection_free(ptr); };

    static void
    throwErrorIfNeeded(CassError const rc, std::string_view const label)
    {
        if (rc == CASS_OK)
            return;
        auto const tag = '[' + std::string{label} + ']';
        throw std::logic_error(tag + ": " + cass_error_desc(rc));
    }

public:
    /* implicit */ Collection(CassCollection* ptr);

    template <typename Type>
    explicit Collection(std::vector<Type> const& value)
        : ManagedObject{cass_collection_new(CASS_COLLECTION_TYPE_LIST, value.size()), kDELETER}
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
    append(ripple::uint256 const& value) const
    {
        auto const rc = cass_collection_append_bytes(
            *this,
            static_cast<cass_byte_t const*>(static_cast<unsigned char const*>(value.data())),
            ripple::uint256::size()
        );
        throwErrorIfNeeded(rc, "Bind ripple::uint256");
    }
};
}  // namespace data::cassandra::impl
