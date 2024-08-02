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

#include "util/Repeat.hpp"

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <mutex>

namespace util {

Timer::Timer(boost::asio::io_context& ioc) : ioc_{ioc}, timer_(ioc_)
{
}

Timer::~Timer()
{
    if (ioc_.stopped())
        return;
    stopping_ = true;
    cancel();
    semaphore_.acquire();
}

void
Timer::cancel()
{
    timer_.cancel();
}

void
Timer::expires_after(std::chrono::steady_clock::duration const& duration)
{
    timer_.expires_after(duration);
}

}  // namespace util
