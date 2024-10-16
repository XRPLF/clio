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

#pragma once

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

#include <cstddef>
#include <functional>

namespace util {

/**
 * @brief CoroutineGroup is a helper class to manage a group of coroutines. It allows to spawn multiple coroutines and
 * wait for all of them to finish.
 */
class CoroutineGroup {
    boost::asio::steady_timer timer_;
    int childrenCounter_{0};

public:
    /**
     * @brief Construct a new Coroutine Group object
     *
     * @param yield The yield context to use for the internal timer
     */
    CoroutineGroup(boost::asio::yield_context yield);

    /**
     * @brief Spawn a new coroutine in the group
     *
     * @param yield The yield context to use for the coroutine (it should be the same as the one used in the
     * constructor)
     * @param fn The function to execute
     */
    void
    spawn(boost::asio::yield_context yield, std::function<void(boost::asio::yield_context)> fn);

    /**
     * @brief Wait for all the coroutines in the group to finish
     *
     * @param yield The yield context to use for the internal timer
     */
    void
    asyncWait(boost::asio::yield_context yield);

    /**
     * @brief Get the number of coroutines in the group
     *
     * @return size_t The number of coroutines in the group
     */
    size_t
    size() const;
};

}  // namespace util
