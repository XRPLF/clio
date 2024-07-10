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

namespace util {

/**
 * @brief Convert a UTC date string to a system_clock::time_point if possible.
 * @param dateStr The UTC date string to convert.
 * @param format The format of the date string.
 * @return The system_clock::time_point if the conversion was successful, otherwise std::nullopt.
 */
[[nodiscard]] std::optional<std::chrono::system_clock::time_point>
SystemTpFromUTCStr(std::string const& dateStr, std::string const& format);

/**
 * @brief Convert a ledger close time which is XRPL network clock to a system_clock::time_point.
 * @param closeTime The ledger close time to convert.
 * @return The system_clock::time_point.
 */
[[nodiscard]] std::chrono::system_clock::time_point
SystemTpFromLedgerCloseTime(ripple::NetClock::time_point closeTime);

}  // namespace util
