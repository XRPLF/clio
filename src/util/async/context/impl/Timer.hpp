#pragma once

#include <boost/asio/steady_timer.hpp>

namespace util::async::impl {

template <typename ExecutorType>
class SteadyTimer {
    boost::asio::steady_timer timer_;

public:
    SteadyTimer(ExecutorType& executor, auto delay, auto&& fn) : timer_{executor}
    {
        timer_.expires_after(delay);
        timer_.async_wait(std::forward<decltype(fn)>(fn));
    }

    SteadyTimer(SteadyTimer&&) = default;

    SteadyTimer(SteadyTimer const&) = delete;

    void
    cancel() noexcept
    {
        timer_.cancel();
    }
};

}  // namespace util::async::impl
