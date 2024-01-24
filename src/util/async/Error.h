
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

#include "util/async/Concepts.h"

#include <fmt/core.h>
#include <fmt/std.h>

#include <exception>
#include <string>
#include <utility>

namespace util::async {

struct ExecutionContextException : std::exception {
    ExecutionContextException(std::string tid, std::string msg)
        : message{fmt::format("Thread {} exit with exception: {}", std::move(tid), std::move(msg))}
    {
    }

    ExecutionContextException(ExecutionContextException const&) = default;
    ExecutionContextException(ExecutionContextException&&) = default;
    ExecutionContextException&
    operator=(ExecutionContextException&&) = default;
    ExecutionContextException&
    operator=(ExecutionContextException const&) = default;

    char const*
    what() const noexcept override
    {
        return message.c_str();
    }

    operator char const*() const noexcept
    {
        return what();
    }

    std::string message;
};

}  // namespace util::async
