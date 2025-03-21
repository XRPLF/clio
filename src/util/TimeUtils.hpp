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
 * This function takes a time_point and converts it to a string using the
 * specified format. The time is interpreted as UTC (Coordinated Universal Time).
 *
 * @param tp The time_point to convert. Must be a valid std::chrono::system_clock::time_point.
 * @param format The format string that specifies the desired output format.
 *        Uses the same format specifiers as strftime:
 *        - %Y: Year as a decimal number (e.g., 2023)
 *        - %m: Month as a decimal number (01-12)
 *        - %d: Day of the month as a decimal number (01-31)
 *        - %H: Hour as a decimal number using a 24-hour clock (00-23)
 *        - %M: Minute as a decimal number (00-59)
 *        - %S: Second as a decimal number (00-60)
 *        - %z: ISO 8601 offset from UTC in timezone (e.g. +0100)
 *        - %Z: Timezone name or abbreviation
 *        - etc.
 *
 * @return A string representation of the time_point formatted according to the provided format.
 *
 * @note This function uses gmtime_r for thread safety when available.
 *
 * @see systemTpFromUtcStr for the inverse operation.
 *
 * @example
 *    auto now = std::chrono::system_clock::now();
 *    std::string isoFormat = "%Y-%m-%dT%H:%M:%SZ";
 *    std::string dateStr = systemTpToUtcStr(now, isoFormat);
 *    // dateStr might be "2023-10-15T14:30:45Z"
 */
[[nodiscard]] std::string
systemTpToUtcStr(std::chrono::system_clock::time_point const& tp, std::string const& format);
;

/**
 * @brief Convert a ledger close time which is XRPL network clock to a system_clock::time_point.
 * @param closeTime The ledger close time to convert.
 * @return The system_clock::time_point.
 */
[[nodiscard]] std::chrono::system_clock::time_point
systemTpFromLedgerCloseTime(ripple::NetClock::time_point closeTime);

}  // namespace util
