//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/config/Config.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <functional>

namespace web {

class BaseDOSGuard;

/**
 * @brief Sweep handler using a steady_timer and boost::asio::io_context.
 */
class IntervalSweepHandler {
    std::chrono::milliseconds sweepInterval_;
    std::reference_wrapper<boost::asio::io_context> ctx_;
    boost::asio::steady_timer timer_;

    web::BaseDOSGuard* dosGuard_ = nullptr;

public:
    /**
     * @brief Construct a new interval-based sweep handler.
     *
     * @param config Clio config to use
     * @param ctx The boost::asio::io_context to use
     */
    IntervalSweepHandler(util::Config const& config, boost::asio::io_context& ctx);

    /**
     * @brief Cancels the sweep timer.
     */
    ~IntervalSweepHandler();

    /**
     * @brief This setup member function is called by @ref BasicDOSGuard during its initialization.
     *
     * @param guard Pointer to the dos guard
     */
    void
    setup(web::BaseDOSGuard* guard);

private:
    void
    createTimer();
};

}  // namespace web
