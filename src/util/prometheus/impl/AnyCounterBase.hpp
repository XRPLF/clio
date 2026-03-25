#pragma once

#include "util/Concepts.hpp"
#include "util/prometheus/impl/CounterImpl.hpp"

#include <concepts>
#include <memory>
#include <type_traits>

namespace util::prometheus::impl {

template <SomeNumberType NumberType>
class AnyCounterBase {
public:
    using ValueType = NumberType;

    template <SomeCounterImpl ImplType = CounterImpl<ValueType>>
        requires std::same_as<ValueType, typename std::remove_cvref_t<ImplType>::ValueType>
    AnyCounterBase(ImplType&& impl = ImplType{})
        : pimpl_(std::make_unique<Model<ImplType>>(std::forward<ImplType>(impl)))
    {
    }

protected:
    struct Concept {
        virtual ~Concept() = default;

        virtual void add(ValueType) = 0;

        virtual void set(ValueType) = 0;

        virtual ValueType
        value() const = 0;
    };

    template <SomeCounterImpl ImplType>
    struct Model : Concept {
        template <SomeCounterImpl SomeImplType>
            requires std::same_as<ImplType, SomeImplType>
        Model(SomeImplType&& impl) : impl(std::forward<SomeImplType>(impl))
        {
        }

        void
        add(ValueType value) override
        {
            impl.add(value);
        }

        void
        set(ValueType v) override
        {
            impl.set(v);
        }

        ValueType
        value() const override
        {
            return impl.value();
        }

        ImplType impl;
    };

    std::unique_ptr<Concept> pimpl_;
};

}  // namespace util::prometheus::impl
