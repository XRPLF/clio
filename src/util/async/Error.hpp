#pragma once

#include <fmt/format.h>
#include <fmt/std.h>

#include <string>
#include <utility>

namespace util::async {

/**
 * @brief Error channel type for async operation of any ExecutionContext
 */
struct ExecutionError {
    /**
     * @brief Construct a new Execution Error object
     *
     * @param tid The thread id
     * @param msg The error message
     */
    ExecutionError(std::string tid, std::string msg)
        : message{fmt::format("Thread {} exit with exception: {}", std::move(tid), std::move(msg))}
    {
    }

    ExecutionError(ExecutionError const&) = default;
    ExecutionError(ExecutionError&&) = default;
    ExecutionError&
    operator=(ExecutionError&&) = default;
    ExecutionError&
    operator=(ExecutionError const&) = default;

    /**
     * @brief Conversion to string
     *
     * @return The error message as a C string
     */
    [[nodiscard]]
    operator char const*() const noexcept
    {
        return message.c_str();
    }

    std::string message;
};

}  // namespace util::async
