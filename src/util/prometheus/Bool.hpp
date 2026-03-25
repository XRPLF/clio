#pragma once

#include "util/Assert.hpp"
#include "util/prometheus/Gauge.hpp"

#include <cstdint>
#include <functional>

namespace util::prometheus {

template <typename T>
concept SomeBoolImpl = requires(T a) {
    { a.set(0) } -> std::same_as<void>;
    { a.value() } -> std::same_as<int64_t>;
};

/**
 * @brief A wrapped to provide bool interface for a Prometheus metric
 * @note Prometheus does not have a native bool type, so we use a counter with a value of 0 or 1
 */
template <SomeBoolImpl ImplType>
class AnyBool {
    std::reference_wrapper<ImplType> impl_;

public:
    /**
     * @brief Construct a bool metric
     *
     * @param impl The implementation of the metric
     */
    explicit AnyBool(ImplType& impl) : impl_(impl)
    {
    }

    /**
     * @brief Set the value of the bool metric
     *
     * @param value The value to set
     * @return A reference to the metric
     */
    AnyBool&
    operator=(bool value)
    {
        impl_.get().set(value ? 1 : 0);
        return *this;
    }

    /**
     * @brief Get the value of the bool metric
     *
     * @return The value of the metric
     */
    operator bool() const
    {
        auto const value = impl_.get().value();
        ASSERT(value == 0 || value == 1, "Invalid value for bool: {}", value);
        return value == 1;
    }
};

/**
 * @brief Alias for Prometheus bool metric with GaugeInt implementation
 */
using Bool = AnyBool<GaugeInt>;

}  // namespace util::prometheus
