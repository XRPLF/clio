#pragma once

#include <xrpl/basics/chrono.h>

#include <chrono>
#include <optional>
#include <string>

namespace util {

/**
 * @brief Convert a UTC date string to a system_clock::time_point if possible.
 * @param dateStr The UTC date string to convert.
 * @param format The format of the date string.
 * @return The system_clock::time_point if the conversion was successful, otherwise std::nullopt.
 */
[[nodiscard]] std::optional<std::chrono::system_clock::time_point>
systemTpFromUtcStr(std::string const& dateStr, std::string const& format);

/**
 * @brief Converts a system_clock time_point to a formatted UTC string.
 *
 * @param tp The time_point to convert. Must be a valid std::chrono::system_clock::time_point.
 * @param format The format string that specifies the desired output format.
 * @return A string representation of the time_point formatted according to the provided format.
 */
[[nodiscard]] std::string
systemTpToUtcStr(std::chrono::system_clock::time_point const& tp, std::string const& format);

/**
 * @brief Convert a ledger close time which is XRPL network clock to a system_clock::time_point.
 * @param closeTime The ledger close time to convert.
 * @return The system_clock::time_point.
 */
[[nodiscard]] std::chrono::system_clock::time_point
systemTpFromLedgerCloseTime(xrpl::NetClock::time_point closeTime);

}  // namespace util
