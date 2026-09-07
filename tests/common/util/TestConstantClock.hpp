#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

/**
 * @brief A clock that always reports the same instant and counts how often it was read.
 *
 * Satisfies util::SomeSystemClock, so it can stand in for std::chrono::system_clock in any
 * component templated on a clock. The call count makes "the clock was sampled exactly once"
 * an assertable property, and the fixed instant makes time-derived output deterministic.
 *
 * The counter is process-wide: reset it in the fixture constructor of every suite that reads it.
 */
class TestConstantClock {
public:
    /** @brief The instant now() reports, as a Unix timestamp in seconds. */
    static constexpr std::uint32_t kNowUnix = 1'700'000'000u;

    /** @brief The instant now() reports. */
    static constexpr std::chrono::system_clock::time_point kNow{std::chrono::seconds{kNowUnix}};

    /**
     * @brief Report the fixed instant and count the read
     *
     * @return kNow
     */
    static std::chrono::system_clock::time_point
    now()
    {
        ++callCounter;
        return kNow;
    }

    /**
     * @brief How often now() has been called since the last reset
     *
     * @return The call count
     */
    static std::size_t
    callCount()
    {
        return callCounter;
    }

    /** @brief Set the call count back to zero. */
    static void
    resetCounter()
    {
        callCounter = 0;
    }

private:
    static inline std::size_t callCounter = 0;
};
