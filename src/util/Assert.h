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

#include <util/SourceLocation.h>

#include <iostream>
#include <string_view>

#include <boost/stacktrace.hpp>
#include <fmt/format.h>

namespace util {

template <typename... Args>
constexpr void
assert_impl(
    SourceLocationType const location,
    char const* expression,
    bool const condition,
    fmt::format_string<Args...> format,
    Args&&... args
)
{
    if (!condition) {
        fmt::println(stderr, "Assertion '{}' failed at {}:{}", expression, location.file_name(), location.line());
        fmt::println(stderr, format, std::forward<Args>(args)...);
        std::cerr << "Stacktrace:\n" << boost::stacktrace::stacktrace() << std::endl;
        std::abort();
    }
}

}  // namespace util

#define ASSERT(condition, ...) util::assert_impl(CURRENT_SRC_LOCATION, #condition, (condition), __VA_ARGS__)
