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

#include <backend/cassandra/Types.h>
#include <backend/cassandra/impl/ManagedObject.h>
#include <backend/cassandra/impl/Tuple.h>
#include <util/Expected.h>

#include <ripple/basics/base_uint.h>
#include <ripple/protocol/STAccount.h>
#include <cassandra.h>
#include <fmt/core.h>

#include <chrono>
#include <compare>
#include <iterator>

namespace Backend::Cassandra::detail {

class Statement : public ManagedObject<CassStatement>
{
    static constexpr auto deleter = [](CassStatement* ptr) { cass_statement_free(ptr); };

    template <typename>
    static constexpr bool unsupported_v = false;

public:
    /**
     * @brief Construct a new statement with optionally provided arguments
     *
     * Note: it's up to the user to make sure the bound parameters match
     * the format of the query (e.g. amount of '?' matches count of args).
     */
    template <typename... Args>
    explicit Statement(std::string_view query, Args&&... args)
        : ManagedObject{cass_statement_new(query.data(), sizeof...(args)), deleter}
    {
        cass_statement_set_consistency(*this, CASS_CONSISTENCY_QUORUM);
        cass_statement_set_is_idempotent(*this, cass_true);
        bind<Args...>(std::forward<Args>(args)...);
    }

    /* implicit */ Statement(CassStatement* ptr) : ManagedObject{ptr, deleter}
    {
        cass_statement_set_consistency(*this, CASS_CONSISTENCY_QUORUM);
        cass_statement_set_is_idempotent(*this, cass_true);
    }

    Statement(Statement&&) = default;

    template <typename... Args>
    void
    bind(Args&&... args) const
    {
        std::size_t idx = 0;
        (this->bindAt<Args>(idx++, std::forward<Args>(args)), ...);
    }

    template <typename Type>
    void
    bindAt(std::size_t const idx, Type&& value) const
    {
        using std::to_string;
        auto throwErrorIfNeeded = [idx](CassError rc, std::string_view label) {
            if (rc != CASS_OK)
                throw std::logic_error(fmt::format("[{}] at idx {}: {}", label, idx, cass_error_desc(rc)));
        };

        auto bindBytes = [this, idx](auto const* data, size_t size) {
            return cass_statement_bind_bytes(*this, idx, static_cast<cass_byte_t const*>(data), size);
        };

        using decayed_t = std::decay_t<Type>;
        using uchar_vec_t = std::vector<unsigned char>;
        using uint_tuple_t = std::tuple<uint32_t, uint32_t>;
        using uint_bytes_tuple_t = std::tuple<uint32_t, ripple::uint256>;

        if constexpr (std::is_same_v<decayed_t, ripple::uint256>)
        {
            auto const rc = bindBytes(value.data(), value.size());
            throwErrorIfNeeded(rc, "Bind ripple::uint256");
        }
        else if constexpr (std::is_same_v<decayed_t, ripple::AccountID>)
        {
            auto const rc = bindBytes(value.data(), value.size());
            throwErrorIfNeeded(rc, "Bind ripple::AccountID");
        }
        else if constexpr (std::is_same_v<decayed_t, uchar_vec_t>)
        {
            auto const rc = bindBytes(value.data(), value.size());
            throwErrorIfNeeded(rc, "Bind vector<unsigned char>");
        }
        else if constexpr (std::is_convertible_v<decayed_t, std::string>)
        {
            // reinterpret_cast is needed here :'(
            auto const rc = bindBytes(reinterpret_cast<unsigned char const*>(value.data()), value.size());
            throwErrorIfNeeded(rc, "Bind string (as bytes)");
        }
        // TODO is there a better way to do this with a generic tuple type?
        else if constexpr (std::is_same_v<decayed_t, uint_tuple_t> || std::is_same_v<decayed_t, uint_bytes_tuple_t>)
        {
            auto const rc = cass_statement_bind_tuple(*this, idx, Tuple{std::move(value)});
            throwErrorIfNeeded(rc, "Bind tuple<uint32, uint32>");
        }
        else if constexpr (std::is_same_v<decayed_t, bool>)
        {
            auto const rc = cass_statement_bind_bool(*this, idx, value ? cass_true : cass_false);
            throwErrorIfNeeded(rc, "Bind bool");
        }
        else if constexpr (std::is_same_v<decayed_t, Limit>)
        {
            auto const rc = cass_statement_bind_int32(*this, idx, value.limit);
            throwErrorIfNeeded(rc, "Bind limit (int32)");
        }
        // clio only uses bigint (int64_t) so we convert any incoming type
        else if constexpr (std::is_convertible_v<decayed_t, int64_t>)
        {
            auto const rc = cass_statement_bind_int64(*this, idx, value);
            throwErrorIfNeeded(rc, "Bind int64");
        }
        else
        {
            // type not supported for binding
            static_assert(unsupported_v<decayed_t>);
        }
    }
};

class PreparedStatement : public ManagedObject<CassPrepared const>
{
    static constexpr auto deleter = [](CassPrepared const* ptr) { cass_prepared_free(ptr); };

public:
    /* implicit */ PreparedStatement(CassPrepared const* ptr) : ManagedObject{ptr, deleter}
    {
    }

    template <typename... Args>
    Statement
    bind(Args&&... args) const
    {
        Statement statement = cass_prepared_bind(*this);
        statement.bind<Args...>(std::forward<Args>(args)...);
        return statement;
    }
};

}  // namespace Backend::Cassandra::detail
