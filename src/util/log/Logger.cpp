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
#include "util/log/PrettyPath.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/formatter.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace util {

bool LogServiceState::isAsync_{true};
Severity LogServiceState::defaultSeverity_{Severity::NFO};
std::vector<spdlog::sink_ptr> LogServiceState::sinks_{};
bool LogServiceState::initialized_{false};

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
    ASSERT(false, "Parsing of log level is incorrect");
    std::unreachable();
}

/**
 * @brief Custom formatter that filters out critical messages
 *
 * This formatter only processes and formats messages with severity level less than critical.
 * Critical messages will be handled separately.
 */
class NonCriticalFormatter : public spdlog::formatter {
public:
    NonCriticalFormatter(std::unique_ptr<spdlog::formatter> wrappedFormatter)
        : wrapped_formatter_(std::move(wrappedFormatter))
    {
    }

    void
    format(spdlog::details::log_msg const& msg, spdlog::memory_buf_t& dest) override
    {
        // Only format messages with severity less than critical
        if (msg.level != spdlog::level::critical) {
            wrapped_formatter_->format(msg, dest);
        }
    }

    std::unique_ptr<formatter>
    clone() const override
    {
        return std::make_unique<NonCriticalFormatter>(wrapped_formatter_->clone());
    }

private:
    std::unique_ptr<spdlog::formatter> wrapped_formatter_;
};

/**
 * @brief Initializes console logging.
 *
 * @param logToConsole A boolean indicating whether to log to console.
 * @param format A string representing the log format.
 * @return Vector of sinks for console logging.
 */
static std::vector<spdlog::sink_ptr>
createConsoleSinks(bool logToConsole, std::string const& format)
{
    std::vector<spdlog::sink_ptr> sinks;

    if (logToConsole) {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(spdlog::level::trace);
        consoleSink->set_formatter(
            std::make_unique<NonCriticalFormatter>(std::make_unique<spdlog::pattern_formatter>(format))
        );
        sinks.push_back(std::move(consoleSink));
    }

    // Always add stderr sink for fatal logs
    auto stderrSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    stderrSink->set_level(spdlog::level::critical);
    stderrSink->set_formatter(std::make_unique<spdlog::pattern_formatter>(format));
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
LogService::createFileSink(FileLoggingParams const& params, std::string const& format)
{
    std::filesystem::path const dirPath(params.logDir);
    // the below are taken from user in MB, but spdlog needs it to be in bytes
    auto const rotationSize = mbToBytes(params.rotationSizeMB);

    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (dirPath / "clio.log").string(), rotationSize, params.dirMaxFiles
    );
    fileSink->set_level(spdlog::level::trace);
    fileSink->set_formatter(std::make_unique<spdlog::pattern_formatter>(format));

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

    auto const overrides = config.getArray("log.channels");

    for (auto it = overrides.begin<util::config::ObjectView>(); it != overrides.end<util::config::ObjectView>(); ++it) {
        auto const& channelConfig = *it;
        auto const name = channelConfig.get<std::string>("channel");
        if (not std::ranges::contains(Logger::kCHANNELS, name)) {
            return std::unexpected{fmt::format("Can't override settings for log channel {}: invalid channel", name)};
        }

        minSeverity[name] = getSeverityLevel(channelConfig.get<std::string>("level"));
    }

    return minSeverity;
}

void
LogServiceState::init(bool isAsync, Severity defaultSeverity, std::vector<spdlog::sink_ptr> const& sinks)
{
    if (initialized_) {
        throw std::logic_error("LogServiceState is already initialized");
    }

    isAsync_ = isAsync;
    defaultSeverity_ = defaultSeverity;
    sinks_ = sinks;
    initialized_ = true;

    spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
        logger->set_level(toSpdlogLevel(defaultSeverity_));
    });

    if (isAsync) {
        static constexpr size_t kQUEUE_SIZE = 8192;
        static constexpr size_t kTHREAD_COUNT = 1;
        spdlog::init_thread_pool(kQUEUE_SIZE, kTHREAD_COUNT);
    }
}

bool
LogServiceState::initialized()
{
    return initialized_;
}

void
LogServiceState::reset()
{
    if (not initialized()) {
        throw std::logic_error("LogService is not initialized");
    }
    isAsync_ = true;
    defaultSeverity_ = Severity::NFO;
    sinks_.clear();
    initialized_ = false;
}

std::shared_ptr<spdlog::logger>
LogServiceState::registerLogger(std::string const& channel, std::optional<Severity> severity)
{
    if (not initialized_) {
        throw std::logic_error("LogService is not initialized");
    }

    std::shared_ptr<spdlog::logger> existingLogger = spdlog::get(channel);
    if (existingLogger != nullptr) {
        if (severity.has_value())
            existingLogger->set_level(toSpdlogLevel(*severity));
        return existingLogger;
    }

    std::shared_ptr<spdlog::logger> logger;
    if (isAsync_) {
        logger = std::make_shared<spdlog::async_logger>(
            channel, sinks_.begin(), sinks_.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block
        );
    } else {
        logger = std::make_shared<spdlog::logger>(channel, sinks_.begin(), sinks_.end());
    }

    logger->set_level(toSpdlogLevel(severity.value_or(defaultSeverity_)));
    logger->flush_on(spdlog::level::err);

    spdlog::register_logger(logger);

    return logger;
}

std::expected<std::vector<spdlog::sink_ptr>, std::string>
LogService::getSinks(config::ClioConfigDefinition const& config)
{
    std::string const format = config.get<std::string>("log.format");

    std::vector<spdlog::sink_ptr> allSinks = createConsoleSinks(config.get<bool>("log.enable_console"), format);

    if (auto const logDir = config.maybeValue<std::string>("log.directory"); logDir.has_value()) {
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
            .rotationSizeMB = config.get<uint32_t>("log.rotation_size"),
            .dirMaxFiles = config.get<uint32_t>("log.directory_max_files"),
        };
        allSinks.push_back(createFileSink(params, format));
    }
    return allSinks;
}

std::expected<void, std::string>
LogService::init(config::ClioConfigDefinition const& config)
{
    auto const sinksMaybe = getSinks(config);
    if (!sinksMaybe.has_value()) {
        return std::unexpected{sinksMaybe.error()};
    }

    LogServiceState::init(
        config.get<bool>("log.is_async"),
        getSeverityLevel(config.get<std::string>("log.level")),
        std::move(sinksMaybe).value()
    );

    // get min severity per channel, can be overridden using the `log.channels` array
    auto const maybeMinSeverity = getMinSeverity(config, defaultSeverity_);
    if (!maybeMinSeverity) {
        return std::unexpected{maybeMinSeverity.error()};
    }
    auto const minSeverity = std::move(maybeMinSeverity).value();

    // Create loggers for each channel
    for (auto const& channel : Logger::kCHANNELS) {
        auto const it = minSeverity.find(channel);
        auto const severity = (it != minSeverity.end()) ? it->second : defaultSeverity_;
        registerLogger(channel, severity);
    }

    spdlog::set_default_logger(spdlog::get("General"));

    LOG(LogService::info()) << "Default log level = " << toString(defaultSeverity_);
    return {};
}

void
LogService::shutdown()
{
    if (initialized_ && isAsync_) {
        // We run in async mode in production, so we need to make sure all logs are flushed before shutting down
        spdlog::shutdown();
    }
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

void
LogServiceState::replaceSinks(std::vector<std::shared_ptr<spdlog::sinks::sink>> const& sinks)
{
    sinks_ = sinks;
    spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) { logger->sinks() = sinks_; });
}

Logger::Logger(std::string channel) : logger_(LogServiceState::registerLogger(channel))
{
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
        spdlog::source_loc const sourceLocation{
            prettyPath(sourceLocation_.file_name()).cbegin(), sourceLocation_.line(), nullptr
        };
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

}  // namespace util
