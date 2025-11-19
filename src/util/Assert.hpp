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

#include <boost/log/core/core.hpp>
#include <fmt/base.h>

#include <functional>
#include <string_view>
#ifndef CLIO_WITHOUT_STACKTRACE
#include <boost/stacktrace.hpp>
#include <boost/stacktrace/stacktrace.hpp>
#endif  // CLIO_WITHOUT_STACKTRACE
#include <fmt/format.h>

#include <cstdlib>

namespace util::impl {

class OnAssert {
public:
    using ActionType = std::function<void(std::string_view)>;

private:
    static ActionType action;

public:
    static void
    call(std::string_view message);

    static void
    setAction(ActionType newAction);

    static void
    resetAction();

private:
    static void
    defaultAction(std::string_view message);
};

/**
 * @brief Assert that a condition is true
 * @note Calls std::exit if the condition is false
 *
 * @tparam Args The format argument types
 * @param location The location of the assertion
 * @param expression The expression to assert
 * @param condition The condition to assert
 * @param format The format string
 * @param args The format arguments
 */
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
#ifndef CLIO_WITHOUT_STACKTRACE
        auto const resultMessage = fmt::format(
            "Assertion '{}' failed at {}:{}:\n{}\nStacktrace:\n{}",
            expression,
            location.file_name(),
            location.line(),
            fmt::format(format, std::forward<Args>(args)...),
            boost::stacktrace::to_string(boost::stacktrace::stacktrace())
        );
#else
        auto const resultMessage = fmt::format(
            "Assertion '{}' failed at {}:{}:\n{}",
            expression,
            location.file_name(),
            location.line(),
            fmt::format(format, std::forward<Args>(args)...)
        );
#endif

        OnAssert::call(resultMessage);
    }
}

}  // namespace util::impl

#define ASSERT(condition, ...) \
    util::impl::assertImpl(CURRENT_SRC_LOCATION, #condition, static_cast<bool>(condition), __VA_ARGS__)
