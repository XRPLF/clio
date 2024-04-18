
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

#include <fmt/core.h>
#include <fmt/std.h>

#include <string>
#include <utility>

// for the static_assert at the bottom which fixes clang compilation:
// see: https://godbolt.org/z/fzTjMd7G1 vs https://godbolt.org/z/jhKG7deen
#include <any>
#include <expected>

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
    [[nodiscard]] operator char const*() const noexcept
    {
        return message.c_str();
    }

    std::string message;
};

// these are not the robots you are looking for...
static_assert(std::is_copy_constructible_v<std::expected<std::any, ExecutionError>>);

}  // namespace util::async
