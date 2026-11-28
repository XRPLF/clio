#pragma once

#include <utility>

namespace util {

/**
 * @brief Run a function when the scope is exited
 *
 * @tparam Func The type of the callable to run; must be invocable with no arguments
 */
template <typename Func>
class ScopeGuard {
public:
    ScopeGuard(ScopeGuard const&) = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    ScopeGuard&
    operator=(ScopeGuard const&) = delete;
    ScopeGuard&
    operator=(ScopeGuard&&) = delete;

    /**
     * @brief Create ScopeGuard object.
     *
     * @param func The function to run when the scope is exited.
     */
    ScopeGuard(Func func) : func_(std::move(func))
    {
    }

    /**
     * @brief Destroy the ScopeGuard, running the stored function.
     */
    ~ScopeGuard()
    {
        func_();
    }

private:
    Func func_;
};

}  // namespace util
