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

#include <fmt/core.h>
#include <fmt/std.h>
#include <util/Expected.h>
#include <util/async/Concepts.h>
#include <util/async/Error.h>
#include <util/async/impl/Any.h>
#include <util/async/impl/ErasedOperation.h>

#include <any>
#include <chrono>
#include <exception>

namespace util::async {

template <typename RetType>
class AnyOperation {
public:
    /* implicit */ AnyOperation(detail::ErasedOperation&& operation) : operation_{std::move(operation)}
    {
    }

    ~AnyOperation() = default;

    AnyOperation(AnyOperation const&) = delete;
    AnyOperation(AnyOperation&&) = default;
    AnyOperation&
    operator=(AnyOperation const&) = delete;
    AnyOperation&
    operator=(AnyOperation&&) = default;

    void
    wait()
    {
        operation_.wait();
    }

    void
    requestStop()
    {
        operation_.requestStop();
    }

    [[nodiscard]] util::Expected<RetType, ExecutionContextException>
    get()
    {
        try {
            auto data = operation_.get();
            if (not data.has_value())
                return util::Unexpected(std::move(data).error());

            return std::any_cast<RetType&>(std::move(data).value());

        } catch (std::bad_any_cast const& e) {
            return util::Unexpected{
                ExecutionContextException(fmt::format("{}", std::this_thread::get_id()), "Bad any cast")
            };
        }
    }

private:
    detail::ErasedOperation operation_;
};

}  // namespace util::async
