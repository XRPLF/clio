#pragma once

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
    std::source_location const location,
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

#define ASSERT(condition, ...)                                                                 \
    util::impl::assertImpl(                                                                    \
        std::source_location::current(), #condition, static_cast<bool>(condition), __VA_ARGS__ \
    )
