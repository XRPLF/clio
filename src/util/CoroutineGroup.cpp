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

#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

namespace util {

CoroutineGroup::CoroutineGroup(boost::asio::yield_context yield, std::optional<size_t> maxChildren)
    : timer_{yield.get_executor(), boost::asio::steady_timer::duration::max()}, maxChildren_{maxChildren}
{
}

CoroutineGroup::~CoroutineGroup()
{
    ASSERT(childrenCounter_ == 0, "CoroutineGroup is destroyed without waiting for child coroutines to finish");
}

bool
CoroutineGroup::spawn(boost::asio::yield_context yield, std::function<void(boost::asio::yield_context)> fn)
{
    if (isFull())
        return false;

    ++childrenCounter_;
    boost::asio::spawn(yield, [this, fn = std::move(fn)](boost::asio::yield_context yield) {
        fn(yield);
        onCoroutineCompleted();
    });
    return true;
}

std::optional<std::function<void()>>
CoroutineGroup::registerForeign()
{
    if (isFull())
        return std::nullopt;

    ++childrenCounter_;
    return [this]() { onCoroutineCompleted(); };
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

bool
CoroutineGroup::isFull() const
{
    return maxChildren_.has_value() && childrenCounter_ >= *maxChildren_;
}

void
CoroutineGroup::onCoroutineCompleted()
{
    ASSERT(childrenCounter_ != 0, "onCoroutineCompleted() called more times than the number of child coroutines");

    --childrenCounter_;
    if (childrenCounter_ == 0)
        timer_.cancel();
}

}  // namespace util
