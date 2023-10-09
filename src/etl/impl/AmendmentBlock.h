//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <etl/SystemState.h>
#include <util/log/Logger.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <functional>

namespace etl::detail {

struct AmendmentBlockAction
{
    void
    operator()()
    {
        static util::Logger const log{"ETL"};
        LOG(log.fatal()) << "Can't process new ledgers: The current ETL source is not compatible with the version of "
                         << "the libxrpl Clio is currently using. Please upgrade Clio to a newer version.";
    }
};

template <typename ActionCallableType = AmendmentBlockAction>
class AmendmentBlockHandler
{
    std::reference_wrapper<boost::asio::io_context> ctx_;
    std::reference_wrapper<SystemState> state_;
    boost::asio::steady_timer timer_;
    std::chrono::milliseconds interval_;

    ActionCallableType action_;

public:
    template <typename DurationType = std::chrono::seconds>
    AmendmentBlockHandler(
        boost::asio::io_context& ioc,
        SystemState& state,
        DurationType interval = DurationType{1},
        ActionCallableType&& action = ActionCallableType())
        : ctx_{std::ref(ioc)}
        , state_{std::ref(state)}
        , timer_{ioc}
        , interval_{std::chrono::duration_cast<std::chrono::milliseconds>(interval)}
        , action_{std::move(action)}
    {
    }

    ~AmendmentBlockHandler()
    {
        boost::asio::post(ctx_.get(), [this]() { timer_.cancel(); });
    }

    void
    onAmendmentBlock()
    {
        state_.get().isAmendmentBlocked = true;
        startReportingTimer();
    }

private:
    void
    startReportingTimer()
    {
        action_();

        timer_.expires_after(interval_);
        timer_.async_wait([this](auto ec) {
            if (!ec)
                boost::asio::post(ctx_.get(), [this] { startReportingTimer(); });
        });
    }
};

}  // namespace etl::detail
