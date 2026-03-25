#pragma once

#include "util/Repeat.hpp"
#include "util/config/ConfigDefinition.hpp"

#include <boost/asio/io_context.hpp>

namespace web::dosguard {

class BaseDOSGuard;

/**
 * @brief Sweep handler clearing context every sweep interval from config.
 */
class IntervalSweepHandler {
    util::Repeat repeat_;

public:
    /**
     * @brief Construct a new interval-based sweep handler.
     *
     * @param config Clio config to use
     * @param ctx The boost::asio::io_context to use
     * @param dosGuard The DOS guard to use
     */
    IntervalSweepHandler(
        util::config::ClioConfigDefinition const& config,
        boost::asio::io_context& ctx,
        web::dosguard::BaseDOSGuard& dosGuard
    );
};

}  // namespace web::dosguard
