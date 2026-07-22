#pragma once

#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>

#include <concepts>
#include <exception>
#include <type_traits>

namespace util {
namespace impl {

template <typename T>
concept IsStrand = std::
    same_as<std::decay_t<T>, boost::asio::strand<typename std::decay_t<T>::inner_executor_type>>;

/**
 * @brief A completion handler that restores `boost::asio::spawn`'s behaviour from Boost 1.83
 *
 * This is intended to be passed as the third argument to `boost::asio::spawn` so that exceptions
 * are not ignored but propagated to `io_context.run()` call site.
 */
inline constexpr struct PropagatingCompletionHandler {
    /**
     * @brief The completion handler
     * @param ePtr The exception that was caught on the coroutine
     */
    void
    operator()(std::exception_ptr ePtr)
    {
        if (ePtr)
            std::rethrow_exception(ePtr);
    }
} kPropagateExceptions;

}  // namespace impl

/**
 * @brief Spawns a coroutine using `boost::asio::spawn`
 *
 * @note This uses kPropagateExceptions to force asio to propagate exceptions through `io_context`
 * @note Since implicit strand was removed from boost::asio::spawn this helper function adds the
 * strand back
 *
 * @tparam Ctx The type of the context/strand
 * @tparam F The type of the function to execute
 * @param ctx The execution context
 * @param func The function to execute. Must return `void`
 */
template <typename Ctx, typename F>
    requires std::is_invocable_r_v<void, F, boost::asio::yield_context>
void
spawn(Ctx&& ctx, F&& func)
{
    if constexpr (impl::IsStrand<Ctx>) {
        boost::asio::spawn(
            std::forward<Ctx>(ctx), std::forward<F>(func), impl::kPropagateExceptions
        );
    } else {
        boost::asio::spawn(
            boost::asio::make_strand(std::forward<Ctx>(ctx).get_executor()),
            std::forward<F>(func),
            impl::kPropagateExceptions
        );
    }
}

}  // namespace util
