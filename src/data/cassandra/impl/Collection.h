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

#pragma once

#include <data/cassandra/impl/ManagedObject.h>

#include <ripple/basics/base_uint.h>
#include <cassandra.h>

#include <string>
#include <string_view>

namespace data::cassandra::detail {

class Collection : public ManagedObject<CassCollection> {
    static constexpr auto deleter = [](CassCollection* ptr) { cass_collection_free(ptr); };

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
        : ManagedObject{cass_collection_new(CASS_COLLECTION_TYPE_LIST, value.size()), deleter}
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
            *this, static_cast<cass_byte_t const*>(static_cast<unsigned char const*>(value.data())), value.size()
        );
        throwErrorIfNeeded(rc, "Bind ripple::uint256");
    }
};
}  // namespace data::cassandra::detail
