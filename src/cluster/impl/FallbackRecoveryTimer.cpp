#include "cluster/impl/FallbackRecoveryTimer.hpp"

#include "util/Mutex.hpp"

#include <boost/asio/thread_pool.hpp>

#include <chrono>
#include <memory>

namespace cluster::impl {

FallbackRecoveryTimer::FallbackRecoveryTimer(
    boost::asio::thread_pool& ctx,
    std::chrono::steady_clock::duration recoveryTime
)
    : impl_(std::make_shared<util::Mutex<Impl>>(Impl{ctx.get_executor(), recoveryTime}))
{
}

bool
FallbackRecoveryTimer::isRunning() const
{
    return impl_->lock()->isRunning;
}

void
FallbackRecoveryTimer::cancel()
{
    auto locked = impl_->lock();
    locked->isRunning = false;
    locked->timer.cancel();
}

}  // namespace cluster::impl
