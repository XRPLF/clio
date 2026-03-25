#pragma once

#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/ManagedObject.hpp"

#include <cassandra.h>

#include <functional>
#include <memory>

namespace data::cassandra::impl {

struct Future : public ManagedObject<CassFuture> {
    /* implicit */ Future(CassFuture* ptr);

    MaybeError
    await() const;

    ResultOrError
    get() const;
};

void
invokeHelper(CassFuture* ptr, void* cbPtr);

class FutureWithCallback : public Future {
public:
    using FnType = std::function<void(ResultOrError)>;
    using FnPtrType = std::unique_ptr<FnType>;

    /* implicit */ FutureWithCallback(CassFuture* ptr, FnType&& cb);
    FutureWithCallback(FutureWithCallback const&) = delete;
    FutureWithCallback(FutureWithCallback&&) = default;

private:
    /** Wrapped in a unique_ptr so it can survive std::move :/ */
    FnPtrType cb_;
};

}  // namespace data::cassandra::impl
