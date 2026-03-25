#include "util/Repeat.hpp"

#include <boost/asio/post.hpp>

namespace util {

void
Repeat::stop()
{
    if (control_->stopping)
        return;

    boost::asio::post(control_->strand, [control = control_] {
        control->stopping = true;
        control->timer.cancel();
    });

    control_->semaphore.acquire();
}

}  // namespace util
