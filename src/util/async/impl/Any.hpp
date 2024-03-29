//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include <any>
#include <type_traits>

// Note: This is a workaround for util::Expected. This is not needed when using std::expected.
// Will be removed after the migration to std::expected is complete (#1173)
// Issue to track this removal can be found here: https://github.com/XRPLF/clio/issues/1174

namespace util::async::impl {

/**
 * @brief A wrapper for std::any to workaround issues with boost.outcome
 */
class Any {
    std::any value_;

public:
    Any() = default;
    Any(Any const&) = default;

    Any(Any&&) = default;
    // note: this needs to be `auto` instead of `std::any` because of a bug in gcc 11.4
    Any(auto&& v)
        requires(std::is_same_v<std::decay_t<decltype(v)>, std::any>)
        : value_{std::forward<decltype(v)>(v)}
    {
    }

    operator std::any&() noexcept
    {
        return value_;
    }
};

}  // namespace util::async::impl
