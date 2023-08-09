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

#include <cassandra.h>

#include <functional>
#include <string>
#include <string_view>
#include <tuple>

namespace data::cassandra::detail {

class Tuple : public ManagedObject<CassTuple>
{
    static constexpr auto deleter = [](CassTuple* ptr) { cass_tuple_free(ptr); };

    template <typename>
    static constexpr bool unsupported_v = false;

public:
    /* implicit */ Tuple(CassTuple* ptr);

    template <typename... Types>
    explicit Tuple(std::tuple<Types...>&& value)
        : ManagedObject{cass_tuple_new(std::tuple_size<std::tuple<Types...>>{}), deleter}
    {
        std::apply(std::bind_front(&Tuple::bind<Types...>, this), std::move(value));
    }

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
            {
                auto const tag = '[' + std::string{label} + ']';
                throw std::logic_error(tag + " at idx " + to_string(idx) + ": " + cass_error_desc(rc));
            }
        };

        using decayed_t = std::decay_t<Type>;

        if constexpr (std::is_same_v<decayed_t, bool>)
        {
            auto const rc = cass_tuple_set_bool(*this, idx, value ? cass_true : cass_false);
            throwErrorIfNeeded(rc, "Bind bool");
        }
        // clio only uses bigint (int64_t) so we convert any incoming type
        else if constexpr (std::is_convertible_v<decayed_t, int64_t>)
        {
            auto const rc = cass_tuple_set_int64(*this, idx, value);
            throwErrorIfNeeded(rc, "Bind int64");
        }
        else
        {
            // type not supported for binding
            static_assert(unsupported_v<decayed_t>);
        }
    }
};

class TupleIterator : public ManagedObject<CassIterator>
{
    template <typename>
    static constexpr bool unsupported_v = false;

public:
    /* implicit */ TupleIterator(CassIterator* ptr);

    [[nodiscard]] static TupleIterator
    fromTuple(CassValue const* value);

    template <typename... Types>
    [[nodiscard]] std::tuple<Types...>
    extract() const
    {
        return {extractNext<Types>()...};
    }

private:
    template <typename Type>
    Type
    extractNext() const
    {
        using std::to_string;
        Type output;

        if (not cass_iterator_next(*this))
            throw std::logic_error("Could not extract next value from tuple iterator");

        auto throwErrorIfNeeded = [](CassError rc, std::string_view label) {
            if (rc != CASS_OK)
            {
                auto const tag = '[' + std::string{label} + ']';
                throw std::logic_error(tag + ": " + cass_error_desc(rc));
            }
        };

        using decayed_t = std::decay_t<Type>;

        // clio only uses bigint (int64_t) so we convert any incoming type
        if constexpr (std::is_convertible_v<decayed_t, int64_t>)
        {
            int64_t out;
            auto const rc = cass_value_get_int64(cass_iterator_get_value(*this), &out);
            throwErrorIfNeeded(rc, "Extract int64 from tuple");
            output = static_cast<decayed_t>(out);
        }
        else
        {
            // type not supported for extraction
            static_assert(unsupported_v<decayed_t>);
        }

        return output;
    }
};

}  // namespace data::cassandra::detail
