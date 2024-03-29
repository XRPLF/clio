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
#include "util/Expected.hpp"
#include "util/async/Concepts.hpp"
#include "util/async/Error.hpp"
#include "util/async/impl/Any.hpp"

#include <memory>
#include <type_traits>

namespace util::async::impl {

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
    wait() noexcept
    {
        pimpl_->wait();
    }

    util::Expected<Any, ExecutionError>
    get()
    {
        return pimpl_->get();
    }

    /**
     * @brief Request the operation to be stopped as soon as possible
     * @note ASSERTs if the operation is not stoppable
     */
    void
    requestStop()
    {
        pimpl_->requestStop();
    }

    /**
     * @brief Cancel the operation if it is scheduled and not yet started
     * @note ASSERTs if the operation is not cancellable
     */
    void
    cancel()
    {
        pimpl_->cancel();
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual void
        wait() noexcept = 0;
        virtual util::Expected<Any, ExecutionError>
        get() = 0;
        virtual void
        requestStop() = 0;
        virtual void
        cancel() = 0;
    };

    template <SomeOperation OpType>
    struct Model : Concept {
        OpType operation;

        template <typename OType>
            requires std::is_same_v<OType, OpType>
        Model(OType&& operation) : operation{std::forward<OType>(operation)}
        {
        }

        void
        wait() noexcept override
        {
            return operation.wait();
        }

        util::Expected<Any, ExecutionError>
        get() override
        {
            // Note: return type of the operation was already wrapped to impl::Any by AnyExecutionContext
            return operation.get();
        }

        void
        requestStop() override
        {
            if constexpr (SomeStoppableOperation<OpType>) {
                operation.requestStop();
            } else {
                ASSERT(false, "Stop requested on non-stoppable operation");
            }
        }

        void
        cancel() override
        {
            if constexpr (SomeCancellableOperation<OpType>) {
                operation.cancel();
            } else {
                ASSERT(false, "Cancellation requested on non-cancellable operation");
            }
        }
    };

private:
    std::unique_ptr<Concept> pimpl_;
};

}  // namespace util::async::impl
