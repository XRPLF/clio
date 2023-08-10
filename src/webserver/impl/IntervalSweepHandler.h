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

#include <boost/asio.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>

namespace web::detail {

/**
 * @brief Sweep handler using a steady_timer and boost::asio::io_context.
 */
class IntervalSweepHandler
{
    std::chrono::milliseconds sweepInterval_;
    std::reference_wrapper<boost::asio::io_context> ctx_;
    boost::asio::steady_timer timer_;

    web::BaseDOSGuard* dosGuard_ = nullptr;

public:
    /**
     * @brief Construct a new interval-based sweep handler
     *
     * @param config Clio config
     * @param ctx The boost::asio::io_context
     */
    IntervalSweepHandler(util::Config const& config, boost::asio::io_context& ctx)
        : sweepInterval_{std::max(1u, static_cast<uint32_t>(config.valueOr("dos_guard.sweep_interval", 1.0) * 1000.0))}
        , ctx_{std::ref(ctx)}
        , timer_{ctx.get_executor()}
    {
    }

    ~IntervalSweepHandler()
    {
        timer_.cancel();
    }

    /**
     * @brief This setup member function is called by @ref BasicDOSGuard during
     * its initialization.
     *
     * @param guard Pointer to the dos guard
     */
    void
    setup(web::BaseDOSGuard* guard)
    {
        assert(dosGuard_ == nullptr);
        dosGuard_ = guard;
        assert(dosGuard_ != nullptr);

        createTimer();
    }

private:
    void
    createTimer()
    {
        timer_.expires_after(sweepInterval_);
        timer_.async_wait([this](boost::system::error_code const& error) {
            if (error == boost::asio::error::operation_aborted)
                return;

            dosGuard_->clear();
            boost::asio::post(ctx_.get().get_executor(), [this] { createTimer(); });
        });
    }
};

}  // namespace web::detail