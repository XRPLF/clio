#pragma once

#include "util/Atomic.hpp"

#include <type_traits>

namespace util::prometheus::impl {

template <typename T>
concept SomeCounterImpl = requires(T a) {
    typename std::remove_cvref_t<T>::ValueType;
    requires SomeNumberType<typename std::remove_cvref_t<T>::ValueType>;
    { a.add(typename std::remove_cvref_t<T>::ValueType{1}) } -> std::same_as<void>;
    { a.set(typename std::remove_cvref_t<T>::ValueType{1}) } -> std::same_as<void>;
    { a.value() } -> SomeNumberType;
};

template <SomeNumberType NumberType>
class CounterImpl {
public:
    using ValueType = NumberType;

    CounterImpl() = default;

    CounterImpl(CounterImpl const&) = delete;

    CounterImpl(CounterImpl&& other) = default;

    CounterImpl&
    operator=(CounterImpl const&) = delete;
    CounterImpl&
    operator=(CounterImpl&&) = default;

    void
    add(ValueType const value)
    {
        value_->add(value);
    }

    void
    set(ValueType const value)
    {
        value_->set(value);
    }

    [[nodiscard]] ValueType
    value() const
    {
        return value_->value();
    }

private:
    AtomicPtr<ValueType> value_ = std::make_unique<Atomic<ValueType>>(0);
};

}  // namespace util::prometheus::impl
