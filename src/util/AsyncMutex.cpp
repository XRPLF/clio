//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "util/AsyncMutex.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

namespace util {

AsyncMutexLock::AsyncMutexLock(AsyncMutex& mutex) : mutex_(mutex)
{
}

AsyncMutexLock::~AsyncMutexLock()
{
    mutex_.unlock();
}

AsyncMutex::AsyncMutex(boost::asio::any_io_executor executor) : timer_{executor}
{
}

AsyncMutexLock
AsyncMutex::lock(boost::asio::yield_context yield)
{
    while (locked_) {
        boost::system::error_code error;
        timer_.async_wait(yield[error]);
    }
    locked_ = true;
    timer_.expires_after(boost::asio::steady_timer::duration::max());
    return AsyncMutexLock(*this);
}

void
AsyncMutex::unlock()
{
    locked_ = false;
    timer_.cancel_one();
}

}  // namespace util
