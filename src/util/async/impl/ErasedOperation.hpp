#pragma once

#include "util/Assert.hpp"
#include "util/async/Concepts.hpp"
#include "util/async/Error.hpp"

#include <any>
#include <expected>
#include <memory>
#include <type_traits>
#include <utility>

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

    void
    invoke()
    {
        pimpl_->invoke();
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
        virtual void
        invoke() = 0;
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
            if constexpr (not SomeAwaitable<OpType>) {
                ASSERT(false, "Called wait() on an operation that does not support it");
                std::unreachable();
            } else {
                operation.wait();
            }
        }

        std::expected<std::any, ExecutionError>
        get() override
        {
            if constexpr (not SomeOperationWithData<OpType>) {
                ASSERT(false, "Called get() on an operation that does not support it");
                std::unreachable();
            } else {
                // Note: return type of the operation was already wrapped to std::any by
                // AnyExecutionContext
                return operation.get();
            }
        }

        void
        abort() override
        {
            if constexpr (
                not SomeCancellableOperation<OpType> and not SomeStoppableOperation<OpType> and
                not SomeAbortable<OpType>
            ) {
                ASSERT(
                    false,
                    "Called abort() on an operation that can't be aborted, cancelled nor stopped"
                );
            } else {
                if constexpr (SomeAbortable<OpType>) {
                    operation.abort();
                } else {
                    if constexpr (SomeCancellableOperation<OpType>)
                        operation.cancel();
                    if constexpr (SomeStoppableOperation<OpType>)
                        operation.requestStop();
                }
            }
        }

        void
        invoke() override
        {
            if constexpr (not SomeForceInvocableOperation<OpType>) {
                ASSERT(false, "Called invoke() on an operation that can't be force-invoked");
            } else {
                operation.invoke();
            }
        }
    };

private:
    std::unique_ptr<Concept> pimpl_;
};

}  // namespace util::async::impl
