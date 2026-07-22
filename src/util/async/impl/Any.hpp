#pragma once

#include <any>
#include <type_traits>

namespace util::async::impl {

/**
 * @brief A wrapper for std::any used as the type-erased transport for async operation results.
 *
 * It exists to work around a recursive-constraint failure in libstdc++'s `<expected>` when the
 * value type is a raw `std::any`: `std::any`'s greedy templated constructor makes
 * `std::expected<std::any, E>` ask whether `std::any` is constructible from
 * `std::expected<std::any, E>`, which re-enters the same constraint. Newer Clang rejects this as
 * self-referential.
 *
 * `Any` only allows construction from `std::any` (never from an `expected`), which breaks the cycle
 * while still allowing transparent unwrapping back to `std::any&`.
 */
class Any {
    std::any value_;

public:
    Any() = default;
    Any(Any const&) = default;

    Any(Any&&) = default;
    // note: this needs to be `auto` instead of `std::any` because of a bug in gcc 11.4
    Any(auto&& v)
        requires(std::is_same_v<std::decay_t<decltype(v)>, std::any>)
        : value_{std::forward<decltype(v)>(v)}
    {
    }

    operator std::any&() noexcept
    {
        return value_;
    }
};

}  // namespace util::async::impl
