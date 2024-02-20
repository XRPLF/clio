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

#include "util/SourceLocation.hpp"
#include "util/log/Logger.hpp"

#include <boost/stacktrace.hpp>
#include <boost/stacktrace/stacktrace.hpp>
#include <fmt/core.h>
#include <fmt/format.h>

#include <cstdlib>

namespace util {

template <typename... Args>
constexpr void
assertImpl(
    SourceLocationType const location,
    char const* expression,
    bool const condition,
    fmt::format_string<Args...> format,
    Args&&... args
)
{
    if (!condition) {
        LOG(LogService::fatal()) << "Assertion '" << expression << "' failed at " << location.file_name() << ":"
                                 << location.line() << ":\n"
                                 << fmt::format(format, std::forward<Args>(args)...) << "\n"
                                 << "Stacktrace:\n"
                                 << boost::stacktrace::stacktrace() << "\n";
        std::exit(EXIT_FAILURE);  // std::abort does not flush gcovr output and causes uncovered lines
    }
}

}  // namespace util

#define ASSERT(condition, ...) util::assertImpl(CURRENT_SRC_LOCATION, #condition, (condition), __VA_ARGS__)
