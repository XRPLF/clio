#pragma once

#include <memory>
#include <stdexcept>

namespace data::cassandra::impl {

template <typename Managed>
class ManagedObject {
protected:
    std::unique_ptr<Managed, void (*)(Managed*)> ptr_;

public:
    template <typename DeleterCallable>
    ManagedObject(Managed* rawPtr, DeleterCallable deleter) : ptr_{rawPtr, deleter}
    {
        if (rawPtr == nullptr)
            throw std::runtime_error("Could not create DB object - got nullptr");
    }

    operator Managed*() const
    {
        return ptr_.get();
    }
};

}  // namespace data::cassandra::impl
