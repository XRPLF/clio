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

#include "util/Expected.h"
#include "util/async/Concepts.h"
#include "util/async/Error.h"

#include <fmt/core.h>
#include <fmt/std.h>

#include <exception>

namespace util::async::detail {

struct DefaultErrorHandler {
    static auto
    wrap(auto&& fn)
    {
        return
            [fn = std::forward<decltype(fn)>(fn)]<typename... Args>(SomeOutcome auto& outcome, Args&&... args) mutable {
                try {
                    fn(outcome, std::forward<Args>(args)...);
                } catch (std::exception const& e) {
                    outcome.setValue(util::Unexpected(
                        ExecutionContextException{fmt::format("{}", std::this_thread::get_id()), e.what()}
                    ));
                } catch (...) {
                    outcome.setValue(util::Unexpected(
                        ExecutionContextException{fmt::format("{}", std::this_thread::get_id()), "unknown"}
                    ));
                }
            };
    }
};

struct NoErrorHandler {
    static auto
    wrap(auto&& fn)
    {
        return std::forward<decltype(fn)>(fn);
    }
};

}  // namespace util::async::detail
