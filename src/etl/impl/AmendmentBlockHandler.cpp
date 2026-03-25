#include "etl/impl/AmendmentBlockHandler.hpp"

#include "etl/SystemState.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <utility>

namespace etl::impl {

AmendmentBlockHandler::ActionType const AmendmentBlockHandler::kDEFAULT_AMENDMENT_BLOCK_ACTION =
    []() {
        static util::Logger const log{"ETL"};  // NOLINT(readability-identifier-naming)
        LOG(
            log.fatal()
        ) << "Can't process new ledgers: The current ETL source is not compatible with the version "
             "of "
          << "the libxrpl Clio is currently using. Please upgrade Clio to a newer version.";
    };

AmendmentBlockHandler::AmendmentBlockHandler(
    util::async::AnyExecutionContext ctx,
    SystemState& state,
    std::chrono::steady_clock::duration interval,
    ActionType action
)
    : state_{std::ref(state)}, interval_{interval}, ctx_{std::move(ctx)}, action_{std::move(action)}
{
}

AmendmentBlockHandler::~AmendmentBlockHandler()
{
    stop();
}

void
AmendmentBlockHandler::notifyAmendmentBlocked()
{
    state_.get().isAmendmentBlocked = true;
    if (not operation_.has_value())
        operation_.emplace(ctx_.executeRepeatedly(interval_, action_));
}

void
AmendmentBlockHandler::stop()
{
    if (operation_.has_value()) {
        operation_->abort();
        operation_.reset();
    }
}

}  // namespace etl::impl
