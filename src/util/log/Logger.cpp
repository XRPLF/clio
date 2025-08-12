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
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ObjectView.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace util {

LogService::Data LogService::data{};

namespace {

spdlog::level::level_enum
toSpdlogLevel(Severity sev)
{
    switch (sev) {
        case Severity::TRC:
            return spdlog::level::trace;
        case Severity::DBG:
            return spdlog::level::debug;
        case Severity::NFO:
            return spdlog::level::info;
        case Severity::WRN:
            return spdlog::level::warn;
        case Severity::ERR:
            return spdlog::level::err;
        case Severity::FTL:
            return spdlog::level::critical;
    }
    return spdlog::level::info;
}

std::string_view
toString(Severity sev)
{
    static constexpr std::array<std::string_view, 6> kLABELS = {
        "TRC",
        "DBG",
        "NFO",
        "WRN",
        "ERR",
        "FTL",
    };

    return kLABELS.at(static_cast<int>(sev));
}

}  // namespace

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

/**
 * @brief Initializes console logging.
 *
 * @param logToConsole A boolean indicating whether to log to console.
 * @return Vector of sinks for console logging.
 */
static std::vector<spdlog::sink_ptr>
createConsoleSinks(bool logToConsole)
{
    std::vector<spdlog::sink_ptr> sinks;

    if (logToConsole) {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(spdlog::level::trace);
        sinks.push_back(std::move(consoleSink));
    }

    // Always add stderr sink for fatal logs
    auto stderrSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    stderrSink->set_level(spdlog::level::critical);
    sinks.push_back(std::move(stderrSink));

    return sinks;
}

/**
 * @brief Initializes file logging.
 *
 * @param config The configuration object containing log settings.
 * @param dirPath The directory path where log files will be stored.
 * @return File sink for logging.
 */
spdlog::sink_ptr
LogService::createFileSink(FileLoggingParams const& params)
{
    std::filesystem::path const dirPath(params.logDir);
    // the below are taken from user in MB, but spdlog needs it to be in bytes
    auto const rotationSize = mbToBytes(params.rotationSizeMB);

    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (dirPath / "clio.log").string(), rotationSize, params.dirMaxFiles
    );
    fileSink->set_level(spdlog::level::trace);

    return fileSink;
}

/**
 * @brief Gets the minimum severity levels for each log channel from the configuration.
 *
 * @param config The configuration object containing log settings.
 * @param defaultSeverity The default severity level to use if not overridden.
 * @return A map of channel names to their minimum severity levels, or an error message if parsing fails.
 */
static std::expected<std::unordered_map<std::string, Severity>, std::string>
getMinSeverity(config::ClioConfigDefinition const& config, Severity defaultSeverity)
{
    std::unordered_map<std::string, Severity> minSeverity;
    for (auto const& channel : Logger::kCHANNELS)
        minSeverity[channel] = defaultSeverity;

    auto const overrides = config.getArray("log_channels");

    for (auto it = overrides.begin<util::config::ObjectView>(); it != overrides.end<util::config::ObjectView>(); ++it) {
        auto const& channelConfig = *it;
        auto const name = channelConfig.get<std::string>("channel");
        if (not std::ranges::contains(Logger::kCHANNELS, name)) {
            return std::unexpected{fmt::format("Can't override settings for log channel {}: invalid channel", name)};
        }

        minSeverity[name] = getSeverityLevel(channelConfig.get<std::string>("log_level"));
    }

    return minSeverity;
}

std::shared_ptr<spdlog::logger>
LogService::registerLogger(std::string const& channel, Severity severity)
{
    std::shared_ptr<spdlog::logger> logger;
    if (data.isAsync) {
        logger = std::make_shared<spdlog::async_logger>(
            channel,
            data.allSinks.begin(),
            data.allSinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block
        );
    } else {
        logger = std::make_shared<spdlog::logger>(channel, data.allSinks.begin(), data.allSinks.end());
    }

    logger->set_level(toSpdlogLevel(severity));
    logger->flush_on(spdlog::level::err);

    spdlog::register_logger(logger);

    return logger;
}

std::expected<void, std::string>
LogService::init(config::ClioConfigDefinition const& config)
{
    // Drop existing loggers
    spdlog::drop_all();

    data.isAsync = config.get<bool>("spdlog_async");

    if (data.isAsync) {
        spdlog::init_thread_pool(8192, 1);
    }

    data.allSinks = createConsoleSinks(config.get<bool>("log_to_console"));

    if (auto const logDir = config.maybeValue<std::string>("log_directory"); logDir.has_value()) {
        std::filesystem::path const dirPath{logDir.value()};
        if (not std::filesystem::exists(dirPath)) {
            if (std::error_code error; not std::filesystem::create_directories(dirPath, error)) {
                return std::unexpected{
                    fmt::format("Couldn't create logs directory '{}': {}", dirPath.string(), error.message())
                };
            }
        }

        FileLoggingParams const params{
            .logDir = logDir.value(),
            .rotationSizeMB = config.get<uint32_t>("log_rotation_size"),
            .dirMaxFiles = config.get<uint32_t>("log_directory_max_files"),
        };
        data.allSinks.push_back(createFileSink(params));
    }

    // get default severity, can be overridden per channel using the `log_channels` array
    auto const defaultSeverity = getSeverityLevel(config.get<std::string>("log_level"));
    auto const maybeMinSeverity = getMinSeverity(config, defaultSeverity);
    if (!maybeMinSeverity) {
        return std::unexpected{maybeMinSeverity.error()};
    }
    auto const minSeverity = std::move(maybeMinSeverity).value();

    // Create loggers for each channel
    for (auto const& channel : Logger::kCHANNELS) {
        auto const it = minSeverity.find(channel);
        auto const severity = (it != minSeverity.end()) ? it->second : defaultSeverity;
        registerLogger(channel, severity);
    }

    spdlog::set_default_logger(spdlog::get("General"));

    std::string const format = config.get<std::string>("spdlog_format");
    spdlog::set_pattern(format);

    LOG(LogService::info()) << "Default log level = " << toString(defaultSeverity);
    return {};
}

void
LogService::shutdown()
{
    spdlog::shutdown();
}

Logger::Pump
LogService::trace(SourceLocationType const& loc)
{
    return Logger(spdlog::default_logger()).trace(loc);
}

Logger::Pump
LogService::debug(SourceLocationType const& loc)
{
    return Logger(spdlog::default_logger()).debug(loc);
}

Logger::Pump
LogService::info(SourceLocationType const& loc)
{
    return Logger(spdlog::default_logger()).info(loc);
}

Logger::Pump
LogService::warn(SourceLocationType const& loc)
{
    return Logger(spdlog::default_logger()).warn(loc);
}

Logger::Pump
LogService::error(SourceLocationType const& loc)
{
    return Logger(spdlog::default_logger()).error(loc);
}

Logger::Pump
LogService::fatal(SourceLocationType const& loc)
{
    return Logger(spdlog::default_logger()).fatal(loc);
}

bool
LogService::enabled()
{
    return spdlog::get_level() != spdlog::level::off;
}

Logger::Logger(std::string channel) : logger_(spdlog::get(channel))
{
    if (!logger_) {
        logger_ = LogService::registerLogger(channel);
    }
}

Logger::Pump::Pump(std::shared_ptr<spdlog::logger> logger, Severity sev, SourceLocationType const& loc)
    : logger_(std::move(logger))
    , severity_(sev)
    , sourceLocation_(loc)
    , enabled_(logger_ != nullptr && logger_->should_log(toSpdlogLevel(sev)))
{
}

Logger::Pump::~Pump()
{
    if (enabled_) {
        spdlog::source_loc const sourceLocation{prettyPath(sourceLocation_).cbegin(), sourceLocation_.line(), nullptr};
        logger_->log(sourceLocation, toSpdlogLevel(severity_), std::move(stream_).str());
    }
}

Logger::Pump
Logger::trace(SourceLocationType const& loc) const
{
    return {logger_, Severity::TRC, loc};
}
Logger::Pump
Logger::debug(SourceLocationType const& loc) const
{
    return {logger_, Severity::DBG, loc};
}
Logger::Pump
Logger::info(SourceLocationType const& loc) const
{
    return {logger_, Severity::NFO, loc};
}
Logger::Pump
Logger::warn(SourceLocationType const& loc) const
{
    return {logger_, Severity::WRN, loc};
}
Logger::Pump
Logger::error(SourceLocationType const& loc) const
{
    return {logger_, Severity::ERR, loc};
}
Logger::Pump
Logger::fatal(SourceLocationType const& loc) const
{
    return {logger_, Severity::FTL, loc};
}

Logger::Logger(std::shared_ptr<spdlog::logger> logger) : logger_(std::move(logger))
{
}

std::string_view
Logger::Pump::prettyPath(SourceLocationType const& loc, size_t maxDepth)
{
    std::string_view const filePath{loc.file_name()};
    auto idx = filePath.size();
    while (maxDepth-- > 0) {
        idx = filePath.rfind('/', idx - 1);
        if (idx == std::string::npos || idx == 0)
            break;
    }
    return filePath.substr(idx == std::string::npos ? 0 : idx + 1);
}

}  // namespace util
