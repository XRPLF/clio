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

#include "etlng/impl/AmendmentBlockHandler.hpp"

#include "etl/SystemState.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <chrono>
#include <functional>
#include <utility>

namespace etlng::impl {

AmendmentBlockHandler::ActionType const AmendmentBlockHandler::kDEFAULT_AMENDMENT_BLOCK_ACTION = []() {
    static util::Logger const log{"ETL"};  // NOLINT(readability-identifier-naming)
    LOG(log.fatal()) << "Can't process new ledgers: The current ETL source is not compatible with the version of "
                     << "the libxrpl Clio is currently using. Please upgrade Clio to a newer version.";
};

AmendmentBlockHandler::AmendmentBlockHandler(
    util::async::AnyExecutionContext&& ctx,
    etl::SystemState& state,
    std::chrono::steady_clock::duration interval,
    ActionType action
)
    : state_{std::ref(state)}, interval_{interval}, ctx_{std::move(ctx)}, action_{std::move(action)}
{
}

void
AmendmentBlockHandler::notifyAmendmentBlocked()
{
    state_.get().isAmendmentBlocked = true;
    if (not operation_.has_value())
        operation_.emplace(ctx_.executeRepeatedly(interval_, action_));
}

}  // namespace etlng::impl
