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

#include <util/prometheus/Metrics.h>

namespace util::prometheus {

template <typename T>
concept SomeNumberType = std::is_arithmetic_v<T> && !std::is_same_v<T, bool> && !std::is_const_v<T>;

// clang-format off
template <typename T>
concept SomeCounterImpl = requires(T a)
{
    typename T::ValueType;
    SomeNumberType<typename T::ValueType>;
    { a.increase(typename T::ValueType{1}) } -> std::same_as<void>;
    { a.reset() } -> std::same_as<void>;
    { a.valueImpl() } -> SomeNumberType;
};
// clang-format on

/**
 * @brief A counter is a cumulative metric that represents a single monotonically increasing counter whose value can
 * only increase or be reset to zero on restart
 */
template <SomeNumberType NumberType>
class AnyCounter : public MetricBase
{
public:
    using ValueType = NumberType;

    /**
     * @brief Construct a new Counter object
     *
     * @param impl The implementation of the counter
     * @param name The name of the counter
     * @param labels The labels of the counter
     * @param description The description of the counter
     */
    template <SomeCounterImpl ImplType>
    requires std::same_as<ValueType, typename ImplType::ValueType>
    AnyCounter(ImplType&& impl, std::string name, std::string labelsString)
        : MetricBase(std::move(name), std::move(labelsString))
        , pimpl_(std::make_unique<Model<ImplType>>(std::forward<ImplType>(impl)))
    {
    }

    /**
     * @brief Increase the counter by one
     */
    AnyCounter&
    operator++()
    {
        pimpl_->increaseImpl(ValueType{1});
        return *this;
    }

    /**
     * @brief Increase the counter by the given value
     *
     * @param value The value to increase the counter by
     */
    AnyCounter&
    operator+=(ValueType const value)
    {
        assert(value >= 0);
        pimpl_->increaseImpl(value);
        return *this;
    }

    /**
     * @brief Reset the counter to zero
     */
    void
    reset()
    {
        pimpl_->resetImpl();
    }

    /**
     * @brief Get the value of the counter
     */
    ValueType
    value() const
    {
        return pimpl_->valueImpl();
    }

    /**
     * @brief Serialize the counter to a string in prometheus format (i.e. name{labels} value)
     *
     * @param result The string to serialize into
     */
    void
    serializeValue(std::string& result) const override
    {
        fmt::format_to(std::back_inserter(result), "{}{} {}", name(), labelsString(), value());
    }

private:
    struct Concept
    {
        virtual ~Concept() = default;

        virtual void increaseImpl(ValueType) = 0;

        virtual void
        resetImpl() = 0;

        virtual ValueType
        valueImpl() const = 0;
    };

    template <SomeCounterImpl ImplType>
    struct Model : Concept
    {
        Model(ImplType&& impl) : impl_(std::move(impl))
        {
        }
        void
        increaseImpl(ValueType value) override
        {
            impl_.increase(value);
        }
        void
        resetImpl() override
        {
            impl_.reset();
        }
        ValueType
        valueImpl() const override
        {
            return impl_.valueImpl();
        }

        ImplType impl_;
    };

    std::unique_ptr<Concept> pimpl_;
};

template <SomeNumberType NumberType>
class CounterImpl
{
public:
    using ValueType = NumberType;

    CounterImpl() = default;

    CounterImpl(CounterImpl const&) = delete;
    CounterImpl(CounterImpl&& other) : value_(other.value_.load())
    {
    }
    CounterImpl&
    operator=(CounterImpl const&) = delete;
    CounterImpl&
    operator=(CounterImpl&&) = delete;

    void
    increase(ValueType value)
    {
        if constexpr (std::is_integral_v<ValueType>)
        {
            value_.fetch_add(value);
        }
        else
        {
#if __cpp_lib_atomic_float >= 201711L
            value_.fetch_add(value);
#else
            // Workaround for atomic float not being supported by the standard library
            // cimpares_exchange_weak returns false if the value is not exchanged and updates the current value
            auto current = value_.load();
            while (!value_.compare_exchange_weak(current, current + value))
            {
            }
#endif
        }
    }

    void
    reset()
    {
        value_ = 0;
    }
    ValueType
    valueImpl() const
    {
        return value_;
    }

private:
    std::atomic<ValueType> value_{0};
};

struct CounterInt : AnyCounter<std::uint64_t>
{
    CounterInt(std::string name, std::string labelsString)
        : AnyCounter(CounterImpl<std::uint64_t>{}, std::move(name), std::move(labelsString))
    {
    }
};

struct CounterDouble : AnyCounter<double>
{
    CounterDouble(std::string name, std::string labelsString)
        : AnyCounter(CounterImpl<double>{}, std::move(name), std::move(labelsString))
    {
    }
};

}  // namespace util::prometheus
