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

#include <fmt/core.h>
#include <fmt/std.h>
#include <util/Expected.h>
#include <util/async/Concepts.h>
#include <util/async/Error.h>
#include <util/async/impl/Any.h>

#include <any>
#include <chrono>
#include <exception>

namespace util::async::detail {

/**
 * @brief A wrapper for std::any to workaround issues with boost.outcome
 */
class Any {
    std::any value_;

public:
    Any() = default;
    Any(std::any&& v) : value_{std::move(v)}
    {
    }

    operator std::any&() noexcept
    {
        return value_;
    }
};

}  // namespace util::async::detail
