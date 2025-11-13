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

#include "util/Assert.hpp"
#include "util/async/Concepts.hpp"
#include "util/async/Error.hpp"

#include <fmt/format.h>
#include <fmt/std.h>

#include <exception>
#include <expected>
#include <thread>

namespace util::async::impl {

struct DefaultErrorHandler {
    [[nodiscard]] static auto
    wrap(auto&& fn) noexcept
    {
        return
            [fn = std::forward<decltype(fn)>(fn)]<typename... Args>(SomeOutcome auto& outcome, Args&&... args) mutable {
                try {
                    std::invoke(std::forward<decltype(fn)>(fn), outcome, std::forward<Args>(args)...);
                } catch (std::exception const& e) {
                    outcome.setValue(
                        std::unexpected(ExecutionError{fmt::format("{}", std::this_thread::get_id()), e.what()})
                    );
                } catch (...) {
                    outcome.setValue(
                        std::unexpected(ExecutionError{fmt::format("{}", std::this_thread::get_id()), "unknown"})
                    );
                }
            };
    }

    [[nodiscard]] static auto
    catchAndAssert(auto&& fn) noexcept  // note this is a lie when used with MockAssert (use MockAssertNoThrow)
    {
        return [fn = std::forward<decltype(fn)>(fn)] mutable {
            try {
                std::invoke(std::forward<decltype(fn)>(fn));
            } catch (std::exception const& e) {
                ASSERT(false, "Exception caught: {}", e.what());
            } catch (...) {
                ASSERT(false, "Unknown exception caught");
            }
        };
    }
};

struct NoErrorHandler {
    [[nodiscard]] static constexpr auto
    wrap(auto&& fn)
    {
        return std::forward<decltype(fn)>(fn);
    }

    [[nodiscard]] static constexpr auto
    catchAndAssert(auto&& fn)
    {
        return std::forward<decltype(fn)>(fn);
    }
};

}  // namespace util::async::impl
