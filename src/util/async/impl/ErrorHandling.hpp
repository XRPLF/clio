#pragma once

#include "util/Assert.hpp"
#include "util/async/Concepts.hpp"
#include "util/async/Error.hpp"

#include <fmt/format.h>
#include <fmt/std.h>

#include <exception>
#include <expected>
#include <thread>

namespace util::async::impl {

struct DefaultErrorHandler {
    [[nodiscard]] static auto
    wrap(auto&& fn) noexcept
    {
        return [fn = std::forward<decltype(fn)>(fn)]<typename... Args>(
                   SomeOutcome auto& outcome, Args&&... args
               ) mutable {
            try {
                std::invoke(std::forward<decltype(fn)>(fn), outcome, std::forward<Args>(args)...);
            } catch (std::exception const& e) {
                outcome.setValue(
                    std::unexpected(
                        ExecutionError{fmt::format("{}", std::this_thread::get_id()), e.what()}
                    )
                );
            } catch (...) {
                outcome.setValue(
                    std::unexpected(
                        ExecutionError{fmt::format("{}", std::this_thread::get_id()), "unknown"}
                    )
                );
            }
        };
    }

    [[nodiscard]] static auto
    catchAndAssert(
        auto&& fn
    ) noexcept  // note this is a lie when used with MockAssert (use MockAssertNoThrow)
    {
        return [fn = std::forward<decltype(fn)>(fn)] mutable {
            try {
                std::invoke(std::forward<decltype(fn)>(fn));
            } catch (std::exception const& e) {
                ASSERT(false, "Exception caught: {}", e.what());
            } catch (...) {
                ASSERT(false, "Unknown exception caught");
            }
        };
    }
};

struct NoErrorHandler {
    [[nodiscard]] static constexpr auto
    wrap(auto&& fn)
    {
        return std::forward<decltype(fn)>(fn);
    }

    [[nodiscard]] static constexpr auto
    catchAndAssert(auto&& fn)
    {
        return std::forward<decltype(fn)>(fn);
    }
};

}  // namespace util::async::impl
