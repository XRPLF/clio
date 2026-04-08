#include "web/dosguard/IntervalSweepHandler.hpp"

#include "util/config/ConfigDefinition.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>

#include <algorithm>
#include <chrono>

namespace web::dosguard {

IntervalSweepHandler::IntervalSweepHandler(
    util::config::ClioConfigDefinition const& config,
    boost::asio::io_context& ctx,
    BaseDOSGuard& dosGuard
)
    : repeat_{ctx}
{
    auto const sweepInterval{std::max(
        std::chrono::milliseconds{1u},
        util::config::ClioConfigDefinition::toMilliseconds(
            config.get<double>("dos_guard.sweep_interval")
        )
    )};
    repeat_.start(sweepInterval, [&dosGuard] { dosGuard.clear(); });
}

}  // namespace web::dosguard
