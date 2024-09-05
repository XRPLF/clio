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

#include "util/async/Concepts.hpp"
#include "util/async/Error.hpp"
#include "util/async/impl/ErasedOperation.hpp"

#include <fmt/core.h>
#include <fmt/std.h>

#include <any>
#include <expected>
#include <thread>
#include <type_traits>
#include <utility>

// TODO: In the future, perhaps cancel and requestStop should be combined into one.
// Users of the library should not care whether the operation is cancellable or stoppable - users just want to cancel
// it whatever that means internally.

namespace util::async {

/**
 * @brief A type-erased operation that can be executed via AnyExecutionContext
 */
template <typename RetType>
class AnyOperation {
public:
    /**
     * @brief Construct a new type-erased Operation object
     *
     * @param operation The operation to wrap
     */
    /* implicit */ AnyOperation(impl::ErasedOperation&& operation) : operation_{std::move(operation)}
    {
    }

    ~AnyOperation() = default;

    AnyOperation(AnyOperation const&) = delete;
    AnyOperation(AnyOperation&&) = default;
    AnyOperation&
    operator=(AnyOperation const&) = delete;
    AnyOperation&
    operator=(AnyOperation&&) = default;

    /**
     * @brief Wait for the operation to complete
     * @note This will block the current thread until the operation is complete
     */
    void
    wait() noexcept
    {
        operation_.wait();
    }

    /**
     * @brief Abort the operation
     *
     * Used to cancel the timer for scheduled operations and request the operation to be stopped as soon as possible
     */
    void
    abort() noexcept
    {
        operation_.abort();
    }

    /**
     * @brief Get the result of the operation
     *
     * @return The result of the operation
     */
    [[nodiscard]] std::expected<RetType, ExecutionError>
    get()
    {
        try {
            auto data = operation_.get();
            if (not data)
                return std::unexpected(std::move(data).error());

            if constexpr (std::is_void_v<RetType>) {
                return {};
            } else {
                return std::any_cast<RetType>(std::move(data).value());
            }

        } catch (std::bad_any_cast const& e) {
            return std::unexpected{ExecutionError(fmt::format("{}", std::this_thread::get_id()), "Bad any cast")};
        }
    }

private:
    impl::ErasedOperation operation_;
};

}  // namespace util::async
