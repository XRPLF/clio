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

#include <any>
#include <expected>
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

    std::expected<std::any, ExecutionError>
    get()
    {
        return pimpl_->get();
    }

    /**
     * @brief Cancel if needed and request stop as soon as possible.
     */
    void
    abort()
    {
        pimpl_->abort();
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual void
        wait() noexcept = 0;
        virtual std::expected<std::any, ExecutionError>
        get() = 0;
        virtual void
        abort() = 0;
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

        std::expected<std::any, ExecutionError>
        get() override
        {
            // Note: return type of the operation was already wrapped to std::any by AnyExecutionContext
            return operation.get();
        }

        void
        abort() override
        {
            if constexpr (not SomeCancellableOperation<OpType> and not SomeStoppableOperation<OpType>) {
                ASSERT(false, "Called abort() on an operation that can't be cancelled nor stopped");
            } else {
                if constexpr (SomeCancellableOperation<OpType>)
                    operation.cancel();
                if constexpr (SomeStoppableOperation<OpType>)
                    operation.requestStop();
            }
        }
    };

private:
    std::unique_ptr<Concept> pimpl_;
};

}  // namespace util::async::impl
