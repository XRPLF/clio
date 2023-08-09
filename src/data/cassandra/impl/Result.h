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
#include <data/cassandra/impl/Tuple.h>
#include <util/Expected.h>

#include <ripple/basics/base_uint.h>
#include <ripple/protocol/AccountID.h>
#include <cassandra.h>

#include <compare>
#include <iterator>
#include <tuple>

namespace data::cassandra::detail {

template <typename>
static constexpr bool unsupported_v = false;

template <typename Type>
inline Type
extractColumn(CassRow const* row, std::size_t idx)
{
    using std::to_string;
    Type output;

    auto throwErrorIfNeeded = [](CassError rc, std::string_view label) {
        if (rc != CASS_OK)
        {
            auto const tag = '[' + std::string{label} + ']';
            throw std::logic_error(tag + ": " + cass_error_desc(rc));
        }
    };

    using decayed_t = std::decay_t<Type>;
    using uint_tuple_t = std::tuple<uint32_t, uint32_t>;
    using uchar_vector_t = std::vector<unsigned char>;

    if constexpr (std::is_same_v<decayed_t, ripple::uint256>)
    {
        cass_byte_t const* buf;
        std::size_t bufSize;
        auto const rc = cass_value_get_bytes(cass_row_get_column(row, idx), &buf, &bufSize);
        throwErrorIfNeeded(rc, "Extract ripple::uint256");
        output = ripple::uint256::fromVoid(buf);
    }
    else if constexpr (std::is_same_v<decayed_t, ripple::AccountID>)
    {
        cass_byte_t const* buf;
        std::size_t bufSize;
        auto const rc = cass_value_get_bytes(cass_row_get_column(row, idx), &buf, &bufSize);
        throwErrorIfNeeded(rc, "Extract ripple::AccountID");
        output = ripple::AccountID::fromVoid(buf);
    }
    else if constexpr (std::is_same_v<decayed_t, uchar_vector_t>)
    {
        cass_byte_t const* buf;
        std::size_t bufSize;
        auto const rc = cass_value_get_bytes(cass_row_get_column(row, idx), &buf, &bufSize);
        throwErrorIfNeeded(rc, "Extract vector<unsigned char>");
        output = uchar_vector_t{buf, buf + bufSize};
    }
    else if constexpr (std::is_same_v<decayed_t, uint_tuple_t>)
    {
        auto const* tuple = cass_row_get_column(row, idx);
        output = TupleIterator::fromTuple(tuple).extract<uint32_t, uint32_t>();
    }
    else if constexpr (std::is_convertible_v<decayed_t, std::string>)
    {
        char const* value;
        std::size_t len;
        auto const rc = cass_value_get_string(cass_row_get_column(row, idx), &value, &len);
        throwErrorIfNeeded(rc, "Extract string");
        output = std::string{value, len};
    }
    else if constexpr (std::is_same_v<decayed_t, bool>)
    {
        cass_bool_t flag;
        auto const rc = cass_value_get_bool(cass_row_get_column(row, idx), &flag);
        throwErrorIfNeeded(rc, "Extract bool");
        output = flag ? true : false;
    }
    // clio only uses bigint (int64_t) so we convert any incoming type
    else if constexpr (std::is_convertible_v<decayed_t, int64_t>)
    {
        int64_t out;
        auto const rc = cass_value_get_int64(cass_row_get_column(row, idx), &out);
        throwErrorIfNeeded(rc, "Extract int64");
        output = static_cast<decayed_t>(out);
    }
    else
    {
        // type not supported for extraction
        static_assert(unsupported_v<decayed_t>);
    }

    return output;
}

struct Result : public ManagedObject<CassResult const>
{
    /* implicit */ Result(CassResult const* ptr);

    [[nodiscard]] std::size_t
    numRows() const;

    [[nodiscard]] bool
    hasRows() const;

    template <typename... RowTypes>
    std::optional<std::tuple<RowTypes...>>
    get() const
        requires(std::tuple_size<std::tuple<RowTypes...>>{} > 1)
    {
        // row managed internally by cassandra driver, hence no ManagedObject.
        auto const* row = cass_result_first_row(*this);
        if (row == nullptr)
            return std::nullopt;

        std::size_t idx = 0;
        auto advanceId = [&idx]() { return idx++; };

        return std::make_optional<std::tuple<RowTypes...>>({extractColumn<RowTypes>(row, advanceId())...});
    }

    template <typename RowType>
    std::optional<RowType>
    get() const
    {
        // row managed internally by cassandra driver, hence no ManagedObject.
        auto const* row = cass_result_first_row(*this);
        if (row == nullptr)
            return std::nullopt;
        return std::make_optional<RowType>(extractColumn<RowType>(row, 0));
    }
};

class ResultIterator : public ManagedObject<CassIterator>
{
    bool hasMore_ = false;

public:
    /* implicit */ ResultIterator(CassIterator* ptr);

    [[nodiscard]] static ResultIterator
    fromResult(Result const& result);

    [[maybe_unused]] bool
    moveForward();

    [[nodiscard]] bool
    hasMore() const;

    template <typename... RowTypes>
    std::tuple<RowTypes...>
    extractCurrentRow() const
    {
        // note: row is invalidated on each iteration.
        // managed internally by cassandra driver, hence no ManagedObject.
        auto const* row = cass_iterator_get_row(*this);

        std::size_t idx = 0;
        auto advanceId = [&idx]() { return idx++; };

        return {extractColumn<RowTypes>(row, advanceId())...};
    }
};

template <typename... Types>
class ResultExtractor
{
    std::reference_wrapper<Result const> ref_;

public:
    struct Sentinel
    {
    };

    struct Iterator
    {
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::size_t;  // rows count
        using value_type = std::tuple<Types...>;

        /* implicit */ Iterator(ResultIterator iterator) : iterator_{std::move(iterator)}
        {
        }

        Iterator(Iterator const&) = delete;
        Iterator&
        operator=(Iterator const&) = delete;

        value_type
        operator*() const
        {
            return iterator_.extractCurrentRow<Types...>();
        }

        value_type
        operator->()
        {
            return iterator_.extractCurrentRow<Types...>();
        }

        Iterator&
        operator++()
        {
            iterator_.moveForward();
            return *this;
        }

        bool
        operator==(Sentinel const&) const
        {
            return not iterator_.hasMore();
        }

    private:
        ResultIterator iterator_;
    };

    ResultExtractor(Result const& result) : ref_{std::cref(result)}
    {
    }

    Iterator
    begin()
    {
        return ResultIterator::fromResult(ref_);
    }

    Sentinel
    end()
    {
        return {};
    }
};

}  // namespace data::cassandra::detail
