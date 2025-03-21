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

#include "util/TimeUtils.hpp"

#include <xrpl/basics/chrono.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace util {
[[nodiscard]] std::optional<std::chrono::system_clock::time_point>
systemTpFromUtcStr(std::string const& dateStr, std::string const& format)
{
    std::tm timeStruct{};
    auto const ret = strptime(dateStr.c_str(), format.c_str(), &timeStruct);
    if (ret == nullptr) {
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(timegm(&timeStruct));
}

[[nodiscard]] std::string
systemTpToUtcStr(std::chrono::system_clock::time_point const& tp, std::string const& format)
{
    auto const timeT = std::chrono::system_clock::to_time_t(tp);
    std::tm timeStruct{};
    gmtime_r(&timeT, &timeStruct);

    std::ostringstream oss;
    oss << std::put_time(&timeStruct, format.c_str());
    return oss.str();
}

[[nodiscard]] std::chrono::system_clock::time_point
systemTpFromLedgerCloseTime(ripple::NetClock::time_point closeTime)
{
    return std::chrono::system_clock::time_point{closeTime.time_since_epoch() + ripple::epoch_offset};
}

}  // namespace util
