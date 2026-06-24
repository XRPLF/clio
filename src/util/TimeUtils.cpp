#include "util/TimeUtils.hpp"

#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <xrpl/basics/chrono.h>

#include <chrono>
#include <ctime>
#include <optional>
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
    auto const formatWrapped = fmt::format("{{:{}}}", format);
    return fmt::format(fmt::runtime(formatWrapped), std::chrono::floor<std::chrono::seconds>(tp));
}

[[nodiscard]] std::chrono::system_clock::time_point
systemTpFromLedgerCloseTime(xrpl::NetClock::time_point closeTime)
{
    return std::chrono::system_clock::time_point{closeTime.time_since_epoch() + xrpl::kEpochOffset};
}

}  // namespace util
