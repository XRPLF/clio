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

#include <util/Expected.h>
#include <util/async/Concepts.h>
#include <util/async/Error.h>
#include <util/async/impl/Any.h>

#include <fmt/core.h>
#include <fmt/std.h>

#include <any>
#include <chrono>
#include <exception>

namespace util::async::detail {

class ErasedOperation {
public:
    template <SomeOperation OpType>
        requires(not std::is_same_v<std::decay_t<OpType>, ErasedOperation>)
    /* implicit */ ErasedOperation(OpType&& operation)
        : pimpl_{std::make_unique<Model<OpType>>(std::forward<OpType>(operation))}
    {
    }

    ~ErasedOperation() = default;

    ErasedOperation(ErasedOperation const&) = delete;
    ErasedOperation(ErasedOperation&&) = default;
    ErasedOperation&
    operator=(ErasedOperation const&) = delete;
    ErasedOperation&
    operator=(ErasedOperation&&) = default;

    void
    wait()
    {
        pimpl_->wait();
    }

    auto
    get()
    {
        return pimpl_->get();
    }

    /**
     * @brief Request the operation to be stopped as soon as possible.
     * @throw std::logic_error if the erased operation is non-cancellable
     */
    void
    requestStop()
    {
        pimpl_->requestStop();
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual void
        wait() = 0;
        virtual util::Expected<detail::Any, ExecutionContextException>
        get() = 0;
        virtual void
        requestStop() = 0;
    };

    template <SomeOperation OpType>
    struct Model : Concept {
        OpType operation;

        Model(OpType&& operation) : operation{std::move(operation)}
        {
        }

        void
        wait() override
        {
            return operation.wait();
        }

        util::Expected<detail::Any, ExecutionContextException>
        get() override
        {
            // Note: return type of the operation was already wrapped to detail::Any by AnyExecutionContext
            return operation.get();
        }

        void
        requestStop() override
        {
            if constexpr (SomeCancellableOperation<OpType>) {
                operation.requestStop();
            } else {
                throw std::logic_error("Cancellation requested on non-cancellable operation");
            }
        }
    };

private:
    std::unique_ptr<Concept> pimpl_;
};

}  // namespace util::async::detail
