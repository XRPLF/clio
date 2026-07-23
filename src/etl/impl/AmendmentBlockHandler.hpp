#pragma once

#include "etl/AmendmentBlockHandlerInterface.hpp"
#include "etl/SystemState.hpp"
#include "util/Mutex.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <functional>
#include <optional>

namespace etl::impl {

class AmendmentBlockHandler : public AmendmentBlockHandlerInterface {
public:
    using ActionType = std::function<void()>;
    using OperationType = std::optional<util::async::AnyOperation<void>>;

private:
    std::reference_wrapper<SystemState> state_;
    std::chrono::steady_clock::duration interval_;
    util::async::AnyExecutionContext ctx_;
    util::Mutex<OperationType> operation_;

    ActionType action_;

public:
    static ActionType const kDefaultAmendmentBlockAction;

    AmendmentBlockHandler(
        util::async::AnyExecutionContext ctx,
        SystemState& state,
        std::chrono::steady_clock::duration interval = std::chrono::seconds{1},
        ActionType action = kDefaultAmendmentBlockAction
    );

    ~AmendmentBlockHandler() override;

    void
    stop() override;

    void
    notifyAmendmentBlocked() override;
};

}  // namespace etl::impl
