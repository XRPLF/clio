//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/CoroutineGroup.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

#include <cstddef>
#include <functional>
#include <utility>

namespace util {

CoroutineGroup::CoroutineGroup(boost::asio::yield_context yield)
    : timer_{yield.get_executor(), boost::asio::steady_timer::duration::max()}
{
}

void
CoroutineGroup::spawn(boost::asio::yield_context yield, std::function<void(boost::asio::yield_context)> fn)
{
    ++childrenCounter_;
    boost::asio::spawn(yield, [this, fn = std::move(fn)](boost::asio::yield_context yield) {
        fn(yield);
        --childrenCounter_;
        if (childrenCounter_ == 0)
            timer_.cancel();
    });
}

void
CoroutineGroup::asyncWait(boost::asio::yield_context yield)
{
    if (childrenCounter_ == 0)
        return;

    boost::system::error_code error;
    timer_.async_wait(yield[error]);
}

size_t
CoroutineGroup::size() const
{
    return childrenCounter_;
}

}  // namespace util
