//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.
    Copyright (c) 2021 Ripple Labs Inc.

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

/*
 * NOTE:
 *
 * This entire file is taken from rippled and modified slightly to fit this
 * codebase as well as fixing the original issue that made this necessary.
 *
 * The reason is that currently there is no easy way to injest the fix that is
 * required to make this implementation correctly work with boost::json::value.
 * Since this will be replaced by `std::expected` as soon as possible there is
 * not much harm done in doing it this way.
 */

#pragma once

#include "util/Assert.h"

#include <boost/outcome.hpp>
#include <ripple/basics/contract.h>

#include <stdexcept>
#include <type_traits>

namespace util {

/** Expected is an approximation of std::expected (hoped for in C++23)

    See: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0323r10.html

    The implementation is entirely based on boost::outcome_v2::result.
*/

// Exception thrown by an invalid access to Expected.
struct bad_expected_access : public std::runtime_error {
    bad_expected_access() : runtime_error("bad expected access")
    {
    }
};

namespace detail {

// Custom policy for Expected.  Always throw on an invalid access.
struct throw_policy : public boost::outcome_v2::policy::base {
    template <class Impl>
    static constexpr void
    wide_value_check(Impl&& self)
    {
        if (!base::_has_value(std::forward<Impl>(self)))
            ripple::Throw<bad_expected_access>();
    }

    template <class Impl>
    static constexpr void
    wide_error_check(Impl&& self)
    {
        if (!base::_has_error(std::forward<Impl>(self)))
            ripple::Throw<bad_expected_access>();
    }

    template <class Impl>
    static constexpr void
    wide_exception_check(Impl&& self)
    {
        if (!base::_has_exception(std::forward<Impl>(self)))
            ripple::Throw<bad_expected_access>();
    }
};

}  // namespace detail

// Definition of Unexpected, which is used to construct the unexpected
// return type of an Expected.
template <class E>
class Unexpected {
public:
    static_assert(!std::is_same_v<E, void>, "E must not be void");

    Unexpected() = delete;

    constexpr explicit Unexpected(E const& e) : val_(e)
    {
    }

    constexpr explicit Unexpected(E&& e) : val_(std::move(e))
    {
    }

    constexpr E const&
    value() const&
    {
        return val_;
    }

    constexpr E&
    value() &
    {
        return val_;
    }

    constexpr E&&
    value() &&
    {
        return std::move(val_);
    }

    constexpr E const&&
    value() const&&
    {
        return std::move(val_);
    }

private:
    E val_;
};

// Unexpected deduction guide that converts array to const*.
template <typename E, std::size_t N>
Unexpected(E (&)[N]) -> Unexpected<E const*>;

// Definition of Expected.  All of the machinery comes from boost::result.
template <class T, class E>
class Expected : private boost::outcome_v2::result<T, E, detail::throw_policy> {
    using Base = boost::outcome_v2::result<T, E, detail::throw_policy>;

public:
    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    constexpr Expected(U r) : Base(T(std::forward<U>(r)))
    {
    }

    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, E>>>
    constexpr Expected(Unexpected<U> e) : Base(E(std::forward<U>(e.value())))
    {
    }

    constexpr bool
    has_value() const
    {
        return Base::has_value();
    }

    constexpr T const&
    value() const&
    {
        return Base::value();
    }

    constexpr T&
    value() &
    {
        return Base::value();
    }

    constexpr T
    value() &&
    {
        return std::move(*base()).value();
    }

    constexpr E const&
    error() const&
    {
        return Base::error();
    }

    constexpr E&
    error() &
    {
        return Base::error();
    }

    constexpr E
    error() &&
    {
        return std::move(*base()).error();
    }

    constexpr explicit
    operator bool() const
    {
        return has_value();
    }

    // Add operator* and operator-> so the Expected API looks a bit more like
    // what std::expected is likely to look like.  See:
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0323r10.html
    [[nodiscard]] constexpr T&
    operator*()
    {
        return this->value();
    }

    [[nodiscard]] constexpr T const&
    operator*() const
    {
        return this->value();
    }

    [[nodiscard]] constexpr T*
    operator->()
    {
        return &this->value();
    }

    [[nodiscard]] constexpr T const*
    operator->() const
    {
        return &this->value();
    }

private:
    Base*
    base()
    {
        auto b = dynamic_cast<Base*>(this);
        ASSERT(b != nullptr, "Base class is not Base");
        return b;
    }
};

// Specialization of Expected<void, E>.  Allows returning either success
// (without a value) or the reason for the failure.
template <class E>
class [[nodiscard]] Expected<void, E> : private boost::outcome_v2::result<void, E, detail::throw_policy> {
    using Base = boost::outcome_v2::result<void, E, detail::throw_policy>;

public:
    // The default constructor makes a successful Expected<void, E>.
    // This aligns with std::expected behavior proposed in P0323R10.
    constexpr Expected() : Base(boost::outcome_v2::success())
    {
    }

    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, E>>>
    constexpr Expected(Unexpected<U> e) : Base(E(std::forward<U>(e.value())))
    {
    }

    constexpr E const&
    error() const
    {
        return Base::error();
    }

    constexpr E&
    error()
    {
        return Base::error();
    }

    constexpr explicit
    operator bool() const
    {
        return Base::has_value();
    }
};

}  // namespace util
