#include "app/Stopper.hpp"

#include "util/Spawn.hpp"

#include <boost/asio/spawn.hpp>

#include <functional>
#include <thread>
#include <utility>

namespace app {

Stopper::~Stopper()
{
    if (worker_.joinable())
        worker_.join();
}

void
Stopper::setOnStop(std::function<void(boost::asio::yield_context)> cb)
{
    util::spawn(ctx_, [this, cb = std::move(cb)](auto yield) {
        cb(yield);

        if (onCompleteCallback_)
            onCompleteCallback_();
    });
}

void
Stopper::setOnComplete(std::function<void()> cb)
{
    onCompleteCallback_ = std::move(cb);
}

void
Stopper::stop()
{
    // Do nothing if worker_ is already running
    if (worker_.joinable())
        return;

    worker_ = std::thread{[this]() { ctx_.run(); }};
}

}  // namespace app
