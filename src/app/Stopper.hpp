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

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>

#include <functional>
#include <thread>
#include <utility>

namespace app {

/**
 * @brief Application stopper class. On stop it will create a new thread to run all the shutdown tasks.
 */
class Stopper {
    boost::asio::io_context ctx_;
    std::thread worker_;

public:
    /**
     * @brief Destroy the Stopper object
     */
    ~Stopper()
    {
        if (worker_.joinable())
            worker_.join();
    }

    /**
     * @brief Set the callabck to be called when the application is stopped.
     *
     * @param cb The callback to be called on application stop.
     */
    void
    setOnStop(std::function<void(boost::asio::yield_context)> cb)
    {
        boost::asio::spawn(ctx_, std::move(cb));
    }

    /**
     * @brief Stop the application and run the shutdown tasks.
     */
    void
    stop()
    {
        // Do nothing if worker_ is already running
        if (worker_.joinable())
            return;

        worker_ = std::thread{[this]() { ctx_.run(); }};
    }
};

}  // namespace app
