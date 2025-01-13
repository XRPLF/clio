//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/log/Logger.hpp"

#include "util/Assert.hpp"
#include "util/BytesConverter.hpp"
#include "util/SourceLocation.hpp"
#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/log/attributes/attribute_value_set.hpp>
#include <boost/log/core/core.hpp>
#include <boost/log/expressions/filter.hpp>
#include <boost/log/keywords/auto_flush.hpp>
#include <boost/log/keywords/file_name.hpp>
#include <boost/log/keywords/filter.hpp>
#include <boost/log/keywords/format.hpp>
#include <boost/log/keywords/max_size.hpp>
#include <boost/log/keywords/open_mode.hpp>
#include <boost/log/keywords/rotation_size.hpp>
#include <boost/log/keywords/target.hpp>
#include <boost/log/keywords/target_file_name.hpp>
#include <boost/log/keywords/time_based_rotation.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <iostream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace util {

Logger LogService::generalLog = Logger{"General"};
Logger LogService::alertLog = Logger{"Alert"};
boost::log::filter LogService::filter{};

std::ostream&
operator<<(std::ostream& stream, Severity sev)
{
    static constexpr std::array<char const*, 6> kLABELS = {
        "TRC",
        "DBG",
        "NFO",
        "WRN",
        "ERR",
        "FTL",
    };

    return stream << kLABELS.at(static_cast<int>(sev));
}

/**
 * @brief converts the loglevel to string to a corresponding Severity enum value.
 *
 * @param logLevel A string representing the log level
 * @return Severity The corresponding Severity enum value.
 */
static Severity
getSeverityLevel(std::string_view logLevel)
{
    if (boost::iequals(logLevel, "trace"))
        return Severity::TRC;
    if (boost::iequals(logLevel, "debug"))
        return Severity::DBG;
    if (boost::iequals(logLevel, "info"))
        return Severity::NFO;
    if (boost::iequals(logLevel, "warning") || boost::iequals(logLevel, "warn"))
        return Severity::WRN;
    if (boost::iequals(logLevel, "error"))
        return Severity::ERR;
    if (boost::iequals(logLevel, "fatal"))
        return Severity::FTL;

    // already checked during parsing of config that value must be valid
    ASSERT(false, "Parsing of log_level is incorrect");
    std::unreachable();
}

void
LogService::init(config::ClioConfigDefinition const& config)
{
    namespace keywords = boost::log::keywords;
    namespace sinks = boost::log::sinks;

    boost::log::add_common_attributes();
    boost::log::register_simple_formatter_factory<Severity, char>("Severity");
    std::string format = config.get<std::string>("log_format");

    if (config.get<bool>("log_to_console")) {
        boost::log::add_console_log(
            std::cout, keywords::format = format, keywords::filter = LogSeverity < Severity::FTL
        );
    }

    // Always print fatal logs to cerr
    boost::log::add_console_log(std::cerr, keywords::format = format, keywords::filter = LogSeverity >= Severity::FTL);

    auto const logDir = config.maybeValue<std::string>("log_directory");
    if (logDir) {
        boost::filesystem::path dirPath{logDir.value()};
        if (!boost::filesystem::exists(dirPath))
            boost::filesystem::create_directories(dirPath);
        auto const rotationPeriod = config.get<uint32_t>("log_rotation_hour_interval");

        // the below are taken from user in MB, but boost::log::add_file_log needs it to be in bytes
        auto const rotationSize = mbToBytes(config.get<uint32_t>("log_rotation_size"));
        auto const dirSize = mbToBytes(config.get<uint32_t>("log_directory_max_size"));
        auto fileSink = boost::log::add_file_log(
            keywords::file_name = dirPath / "clio.log",
            keywords::target_file_name = dirPath / "clio_%Y-%m-%d_%H-%M-%S.log",
            keywords::auto_flush = true,
            keywords::format = format,
            keywords::open_mode = std::ios_base::app,
            keywords::rotation_size = rotationSize,
            keywords::time_based_rotation =
                sinks::file::rotation_at_time_interval(boost::posix_time::hours(rotationPeriod))
        );
        fileSink->locked_backend()->set_file_collector(
            sinks::file::make_collector(keywords::target = dirPath, keywords::max_size = dirSize)
        );
        fileSink->locked_backend()->scan_for_files();
    }

    // get default severity, can be overridden per channel using the `log_channels` array
    auto const defaultSeverity = getSeverityLevel(config.get<std::string>("log_level"));

    std::unordered_map<std::string, Severity> minSeverity;
    for (auto const& channel : Logger::kCHANNELS)
        minSeverity[channel] = defaultSeverity;
    minSeverity["Alert"] = Severity::WRN;  // Channel for alerts, always warning severity

    auto const overrides = config.getArray("log_channels");

    for (auto it = overrides.begin<util::config::ObjectView>(); it != overrides.end<util::config::ObjectView>(); ++it) {
        auto const& channelConfig = *it;
        auto const name = channelConfig.get<std::string>("channel");
        if (std::count(std::begin(Logger::kCHANNELS), std::end(Logger::kCHANNELS), name) == 0)
            throw std::runtime_error("Can't override settings for log channel " + name + ": invalid channel");

        minSeverity[name] = getSeverityLevel(channelConfig.get<std::string>("log_level"));
    }

    auto logFilter = [minSeverity = std::move(minSeverity),
                      defaultSeverity](boost::log::attribute_value_set const& attributes) -> bool {
        auto const channel = attributes[LogChannel];
        auto const severity = attributes[LogSeverity];
        if (!channel || !severity)
            return false;
        if (auto const it = minSeverity.find(channel.get()); it != minSeverity.end())
            return severity.get() >= it->second;
        return severity.get() >= defaultSeverity;
    };

    filter = boost::log::filter{std::move(logFilter)};
    boost::log::core::get()->set_filter(filter);
    LOG(LogService::info()) << "Default log level = " << defaultSeverity;
}

Logger::Pump
Logger::trace(SourceLocationType const& loc) const
{
    return {logger_, Severity::TRC, loc};
};
Logger::Pump
Logger::debug(SourceLocationType const& loc) const
{
    return {logger_, Severity::DBG, loc};
};
Logger::Pump
Logger::info(SourceLocationType const& loc) const
{
    return {logger_, Severity::NFO, loc};
};
Logger::Pump
Logger::warn(SourceLocationType const& loc) const
{
    return {logger_, Severity::WRN, loc};
};
Logger::Pump
Logger::error(SourceLocationType const& loc) const
{
    return {logger_, Severity::ERR, loc};
};
Logger::Pump
Logger::fatal(SourceLocationType const& loc) const
{
    return {logger_, Severity::FTL, loc};
};

std::string
Logger::Pump::prettyPath(SourceLocationType const& loc, size_t maxDepth)
{
    auto const filePath = std::string{loc.file_name()};
    auto idx = filePath.size();
    while (maxDepth-- > 0) {
        idx = filePath.rfind('/', idx - 1);
        if (idx == std::string::npos || idx == 0)
            break;
    }
    return filePath.substr(idx == std::string::npos ? 0 : idx + 1) + ':' + std::to_string(loc.line());
}

}  // namespace util
