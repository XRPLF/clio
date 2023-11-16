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

#include <iostream>
#include <string_view>

#include <fmt/format.h>

namespace util {

template <typename... Args>
constexpr void
assert_impl(
    char const* file,
    int line,
    char const* expression,
    bool const condition,
    fmt::format_string<Args...> format,
    Args&&... args
)
{
    if (!condition) {
        fmt::println(stderr, "Assertion ({}) failed at {}: {}", expression, file, line);
        if constexpr (sizeof...(args) > 0)
            fmt::println(stderr, format, std::forward<Args>(args)...);
        std::abort();
    }
}

}  // namespace util

#define ASSERT(condition, message, ...) \
    util::assert_impl(__FILE__, __LINE__, #condition, (condition), (message), __VA_ARGS__);
