//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <concepts>
#include <type_traits>

namespace util {

template <typename T>
concept SomeAtomic =
    std::same_as<std::remove_cvref_t<T>, std::atomic<std::remove_cvref_t<typename T::value_type>>>;

/**
 * @brief Concept defining types that can be observed for changes.
 *
 * A type is Observable if it satisfies all requirements for being stored
 * and monitored in an ObservableValue container:
 *
 * - Must be equality comparable to detect changes
 * - Must be copy constructible for capturing old values in guards
 * - Must be move constructible for efficient value updates
 *
 * @note Copy assignment is intentionally not required since we use move semantics
 *       for value updates and only need copy construction for change detection.
 */
template <typename T>
concept Observable =
    std::equality_comparable<T> && std::copy_constructible<T> && std::move_constructible<T>;

namespace impl {

/**
 * @brief Base class containing common ObservableValue functionality.
 *
 * This class contains all the observer management and notification logic
 * that is shared between regular and atomic ObservableValue specializations.
 *
 * @tparam T The value type (for atomic specializations, this is the underlying type, not
 * std::atomic<T>)
 */
template <Observable T>
class ObservableValueBase {
protected:
    boost::signals2::signal<void(T const&)> onUpdate_;

public:
    virtual ~ObservableValueBase() = default;

    /**
     * @brief Registers an observer callback for value changes.
     * @param fn Callback function/lambda that accepts T const&
     * @return Connection object for managing the subscription
     */
    boost::signals2::connection
    observe(std::invocable<T const&> auto&& fn)
    {
        return onUpdate_.connect(std::forward<decltype(fn)>(fn));
    }

    /**
     * @brief Checks if there are any active observers.
     * @return true if there are observers, false otherwise
     */
    [[nodiscard]] bool
    hasObservers() const
    {
        return not onUpdate_.empty();
    }

    /**
     * @brief Forces notification of all observers with the current value.
     *
     * This method will notify all observers with the current value regardless
     * of whether the value has changed since the last notification.
     */
    virtual void
    forceNotify() = 0;

protected:
    /**
     * @brief Notifies all observers with the given value.
     * @param value The value to send to observers
     */
    void
    notifyObservers(T const& value)
    {
        onUpdate_(value);
    }
};

}  // namespace impl

// Forward declaration
template <typename T>
class ObservableValue;

/**
 * @brief An observable value container that notifies observers when the value changes.
 *
 * ObservableValue wraps a value of type T and provides a mechanism to observe changes to that
 * value. When the value is modified (and actually changes), all registered observers are notified.
 *
 * @tparam T The type of value to observe. Must satisfy the Observable concept.
 *
 * @par Thread Safety
 * - Observer subscription/unsubscription (observe() and connection.disconnect()) are thread-safe
 * - Value modification operations (set(), operator=) are NOT thread-safe and require external
 * synchronization
 * - Observer callbacks are invoked synchronously on the same thread that triggered the value change
 * - If observers need to perform work on different threads, they must handle dispatch themselves
 *   (e.g., using an async execution context or message queue)
 *
 * @par Exception Handling
 * - If an observer callback throws an exception, the exception will propagate to the caller
 * - The value will still be updated even if observers throw exceptions
 * - No guarantee is made about whether other observers will be called if one throws
 * - It is the caller's responsibility to handle exceptions from observer callbacks
 */
template <Observable T>
    requires(not SomeAtomic<T>)
class ObservableValue<T> : public impl::ObservableValueBase<T> {
    T value_;

    /**
     * @brief RAII guard for deferred notification of value changes.
     *
     * ObservableGuard captures the current value when created and compares it
     * with the final value when destroyed. If the values differ, observers
     * are notified. This allows for multiple modifications to the value with
     * only a single notification at the end.
     *
     * @note This class is returned by operator->() and should not be used directly.
     */
    struct ObservableGuard {
        T const oldValue;         ///< Value captured at construction time
        ObservableValue<T>& ref;  ///< Reference to the observable value

        /**
         * @brief Constructs guard and captures current value.
         * @param observable The ObservableValue to guard
         */
        ObservableGuard(ObservableValue<T>& observable) : oldValue(observable), ref(observable)
        {
        }

        /**
         * @brief Destructor that triggers notification if value changed.
         *
         * Compares the captured value with the current value. If they differ,
         * notifies all observers with the current value.
         */
        ~ObservableGuard()
        {
            if (oldValue != ref.value_)
                ref.notifyObservers(ref.value_);
        }

        /**
         * @brief Provides mutable access to the underlying value.
         * @return Mutable reference to the wrapped value
         */
        [[nodiscard]]
        operator T&()
        {
            return ref.value_;
        }
    };

public:
    /**
     * @brief Constructs ObservableValue with initial value.
     * @param value Initial value (must be convertible to T)
     */
    ObservableValue(std::convertible_to<T> auto&& value)
        : value_{std::forward<decltype(value)>(value)}
    {
    }

    /**
     * @brief Constructs ObservableValue with default initial value.
     */
    ObservableValue()
        requires std::default_initializable<T>
        : value_{}
    {
    }

    ObservableValue(ObservableValue const&) = delete;
    ObservableValue(ObservableValue&&) = default;
    ObservableValue&
    operator=(ObservableValue const&) = delete;
    ObservableValue&
    operator=(ObservableValue&&) = default;

    /**
     * @brief Assignment operator that updates value and notifies observers.
     *
     * Updates the stored value and notifies observers if the new value
     * differs from the current value (using operator!=).
     *
     * @param val New value (must be convertible to T)
     * @return Reference to this object for chaining
     *
     * @throws Any exception thrown by observer callbacks will propagate
     */
    ObservableValue&
    operator=(std::convertible_to<T> auto&& val)
    {
        set(val);
        return *this;
    }

    /**
     * @brief Provides deferred notification access to the value.
     *
     * Returns an ObservableGuard that allows modification of the value
     * with notification deferred until the guard is destroyed.
     *
     * @return ObservableGuard for deferred notification
     */
    [[nodiscard]] ObservableGuard
    operator->()
    {
        return {*this};
    }

    /**
     * @brief Implicit conversion to const reference of the value.
     * @return Const reference to the stored value
     */
    [[nodiscard]]
    operator T const&() const
    {
        return value_;
    }

    /**
     * @brief Explicitly gets the current value.
     * @return Const reference to the stored value
     */
    [[nodiscard]] T const&
    get() const
    {
        return value_;
    }

    /**
     * @brief Sets a new value and notifies observers if changed.
     *
     * Updates the stored value and notifies all observers if the new value
     * differs from the current value (using operator!=). If the values are
     * equal, no notification occurs.
     *
     * @param val New value (must be convertible to T)
     *
     * @throws Any exception thrown by observer callbacks will propagate
     *
     * @par Thread Safety
     * - This method is NOT thread-safe and requires external synchronization for concurrent access
     * - Observer callbacks are invoked synchronously on the calling thread
     */
    void
    set(std::convertible_to<T> auto&& val)
    {
        if (value_ != val) {
            value_ = std::forward<decltype(val)>(val);
            this->notifyObservers(value_);
        }
    }

    /**
     * @brief Forces notification of all observers with the current value.
     *
     * This method will notify all observers with the current value regardless
     * of whether the value has changed since the last notification.
     */
    void
    forceNotify() override
    {
        this->notifyObservers(value_);
    }
};

/**
 * @brief Partial specialization of ObservableValue for atomic types.
 *
 * This specialization provides thread-safe observation of atomic values while
 * maintaining atomic semantics. It avoids the issues of copying atomic values
 * and handles race conditions properly.
 *
 * @tparam T The underlying type stored in the atomic
 *
 * @par Thread Safety
 * - All operations are thread-safe
 * - Observer notifications are atomic with respect to value changes
 * - Multiple threads can safely modify and observe the atomic value
 *
 * @par Performance Considerations
 * - Uses atomic compare-and-swap operations for updates
 * - Minimizes atomic reads during guard operations
 * - Observer notifications happen outside of atomic operations when possible
 */
template <Observable T>
class ObservableValue<std::atomic<T>> : public impl::ObservableValueBase<T> {
    std::atomic<T> value_;

public:
    /**
     * @brief Constructs ObservableValue with initial atomic value.
     * @param value Initial value (will be stored in the atomic)
     */
    ObservableValue(std::convertible_to<T> auto&& value)
        : value_{std::forward<decltype(value)>(value)}
    {
    }

    /**
     * @brief Constructs ObservableValue with default initial value.
     */
    ObservableValue()
        requires std::default_initializable<T>
        : value_{}
    {
    }

    ObservableValue(ObservableValue const&) = delete;
    ObservableValue(ObservableValue&&) = default;
    ObservableValue&
    operator=(ObservableValue const&) = delete;
    ObservableValue&
    operator=(ObservableValue&&) = default;

    /**
     * @brief Assignment operator that updates atomic value and notifies observers.
     *
     * Uses atomic compare-and-swap to update the value and notifies observers
     * only if the value actually changed.
     *
     * @param val New value
     * @return Reference to this object for chaining
     */
    ObservableValue&
    operator=(std::convertible_to<T> auto&& val)
    {
        set(std::forward<decltype(val)>(val));
        return *this;
    }

    /**
     * @brief Gets the current atomic value.
     * @return Current value stored in the atomic
     */
    [[nodiscard]] T
    get() const
    {
        return value_.load();
    }

    /**
     * @brief Implicit conversion to the current atomic value.
     * @return Current value stored in the atomic
     */
    [[nodiscard]]
    operator T() const
    {
        return get();
    }

    /**
     * @brief Sets a new atomic value and notifies observers if changed.
     *
     * Uses atomic compare-and-swap to update the value. Notifies all observers
     * if the value actually changed.
     *
     * @param val New value
     */
    void
    set(std::convertible_to<T> auto&& val)
    {
        T newValue = std::forward<decltype(val)>(val);
        T oldValue = value_.load();

        // Use compare-and-swap to atomically update
        while (!value_.compare_exchange_weak(oldValue, newValue)) {
            // compare_exchange_weak updates oldValue with current value on failure
            // Continue until we succeed
        }

        // Notify observers if we actually changed the value
        // Note: oldValue now contains the actual previous value that was replaced
        if (oldValue != newValue) {
            this->notifyObservers(newValue);
        }
    }

    /**
     * @brief Forces notification of all observers with the current value.
     *
     * This method will notify all observers with the current atomic value
     * regardless of whether the value has changed since the last notification.
     */
    void
    forceNotify() override
    {
        this->notifyObservers(value_.load());
    }
};

}  // namespace util
