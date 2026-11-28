#pragma once

#include <boost/asio/spawn.hpp>

#include <chrono>
#include <concepts>
#include <functional>
#include <optional>
#include <type_traits>

namespace util::async {

/**
 * @brief Tag type for identifying execution context types.
 *
 * Types that inherit from this tag can be detected using the SomeExecutionContext concept.
 * This allows generic code to differentiate between raw Boost.Asio contexts and wrapped execution
 * contexts.
 */
struct ExecutionContextTag {
    virtual ~ExecutionContextTag() = default;
};

/**
 * @brief Concept that identifies types derived from ExecutionContextTag.
 *
 * This concept is used to detect custom execution context wrappers (like BasicExecutionContext)
 * and distinguish them from raw Boost.Asio contexts (io_context, thread_pool, etc.).
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeExecutionContext = std::derived_from<std::remove_cvref_t<T>, ExecutionContextTag>;

/**
 * @brief Specifies the interface for an entity that can be stopped
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeStoppable = requires(T v) {
    { v.requestStop() } -> std::same_as<void>;
};

/**
 * @brief Specifies the interface for an entity that can be cancelled
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeCancellable = requires(T v) {
    { v.cancel() } -> std::same_as<void>;
};

/**
 * @brief Specifies the interface for an operation that can be awaited
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeAwaitable = requires(T v) {
    { v.wait() } -> std::same_as<void>;
};

/**
 * @brief Specifies the interface for an operation that can be aborted
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeAbortable = requires(T v) {
    { v.abort() } -> std::same_as<void>;
};

/**
 * @brief Specifies the interface for an operation
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeOperation = SomeAwaitable<T> or SomeAbortable<T>;

/**
 * @brief Specifies the interface for an operation that produces data
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeOperationWithData = SomeOperation<T> and requires(T v) {
    { v.get() };
};

/**
 * @brief Specifies the interface for an operation that can force-invoked
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeForceInvocableOperation = SomeOperation<T> and requires(T v) {
    { v.invoke() };
};

/**
 * @brief Specifies the interface for an operation that can be stopped
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeStoppableOperation = SomeOperation<T> and SomeStoppable<T>;

/**
 * @brief Specifies the interface for an operation that can be cancelled
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeCancellableOperation = SomeOperation<T> and SomeCancellable<T>;

/**
 * @brief Specifies the interface for an outcome (promise)
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeOutcome = requires(T v) {
    { v.getOperation() } -> SomeOperation;
};

/**
 * @brief Specifies the interface for a stop token
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeStopToken = requires(T v) {
    { v.isStopRequested() } -> std::same_as<bool>;
};

/**
 * @brief Specifies the interface for a stop source that produces tokens from a
 * boost::asio::yield_context
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeYieldStopSource = requires(T v, boost::asio::yield_context yield) {
    { v[yield] } -> SomeStopToken;
};

/**
 * @brief Specifies the interface for a simple stop source
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeSimpleStopSource = requires(T v) {
    { v.getToken() } -> SomeStopToken;
};

/**
 * @brief Specifies the interface for a stop source
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeStopSource = (SomeSimpleStopSource<T> or SomeYieldStopSource<T>) and SomeStoppable<T>;

/**
 * @brief Specifies the interface for a provider of stop sources
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeStopSourceProvider = requires(T v) {
    { v.getStopSource() } -> SomeStopSource;
};

/**
 * @brief Specifies the interface for an outcome (promise) that can be stopped
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeStoppableOutcome = SomeOutcome<T> and SomeStopSourceProvider<T>;

/**
 * @brief Specifies the interface for a handler that is invocable without a stop token
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeHandlerWithoutStopToken = requires(T fn) {
    { std::invoke(fn) };
};

/**
 * @brief Specifies the interface for a handler that can be invoked with the specified args
 *
 * @tparam T The handler type to check
 * @tparam Args The argument types the handler must be invocable with
 */
template <typename T, typename... Args>
concept SomeHandlerWith = requires(T fn) {
    { std::invoke(fn, std::declval<Args>()...) };
};

/**
 * @brief Specifies that the type must be some std::duration
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeStdDuration = requires {
    // Thank you Ed Catmur for this trick.
    // See
    // https://stackoverflow.com/questions/74383254/concept-that-models-only-the-stdchrono-duration-types
    []<typename Rep, typename Period>(  //
        std::type_identity<std::chrono::duration<Rep, Period>>
    ) {}(std::type_identity<std::decay_t<T>>());
};

/**
 * @brief Specifies that the type must be some std::optional
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeStdOptional = requires {
    []<typename Type>(  //
        std::type_identity<std::optional<Type>>
    ) {}(std::type_identity<std::decay_t<T>>());
};

/**
 * @brief Specifies that the type must be some std::duration wrapped in an optional
 *
 * @tparam T The type to check
 */
template <typename T>
concept SomeOptStdDuration = SomeStdOptional<T> and SomeStdDuration<decltype(T{}.value())>;

/**
 * @brief Checks that decayed T is not of the same type as Erased
 *
 * @tparam T The type to check
 * @tparam Erased The type T must differ from after decay
 */
template <typename T, typename Erased>
concept NotSameAs = not std::is_same_v<std::decay_t<T>, Erased>;

/**
 * @brief Check that T is an r-value and is not the same type as Erased
 *
 * @tparam T The type to check
 * @tparam Erased The type T must differ from after decay
 */
template <typename T, typename Erased>
concept RValueNotSameAs = requires(T&& t) {
    requires std::is_rvalue_reference_v<decltype(t)>;
    requires NotSameAs<T, Erased>;
};

}  // namespace util::async
