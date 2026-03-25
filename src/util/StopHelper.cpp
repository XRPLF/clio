#include "util/StopHelper.hpp"

#include "util/Spawn.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>

namespace util {

void
StopHelper::readyToStop()
{
    onStopReady_();
    *stopped_ = true;
}

void
StopHelper::asyncWaitForStop(boost::asio::yield_context yield)
{
    boost::asio::steady_timer timer{
        yield.get_executor(), std::chrono::steady_clock::duration::max()
    };
    onStopReady_.connect([&]() { util::spawn(yield, [&timer](auto&&) { timer.cancel(); }); });
    boost::system::error_code error;
    if (!*stopped_)
        timer.async_wait(yield[error]);
}

}  // namespace util
