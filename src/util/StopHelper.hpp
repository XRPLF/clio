#pragma once

#include <boost/asio/spawn.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <memory>

namespace util {

/**
 * @brief Helper class to stop a class asynchronously.
 */
class StopHelper {
    boost::signals2::signal<void()> onStopReady_;
    std::unique_ptr<std::atomic_bool> stopped_ = std::make_unique<std::atomic_bool>(false);

public:
    StopHelper() = default;
    ~StopHelper() = default;

    StopHelper(StopHelper&&) = delete;
    StopHelper&
    operator=(StopHelper&&) = delete;
    StopHelper(StopHelper const&) = delete;
    StopHelper&
    operator=(StopHelper const&) = delete;

    /**
     * @brief Notify that the class is ready to stop.
     */
    void
    readyToStop();

    /**
     * @brief Wait for the class to stop.
     *
     * @param yield The coroutine context
     */
    void
    asyncWaitForStop(boost::asio::yield_context yield);
};

}  // namespace util
