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

#pragma once

#include "util/SourceLocation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// We forward declare spdlog::logger and spdlog::sinks::sink
// to avoid including the spdlog headers in this header file.
namespace spdlog {

class logger;  // NOLINT(readability-identifier-naming)

namespace sinks {
class sink;  // NOLINT(readability-identifier-naming)
}  // namespace sinks

}  // namespace spdlog

struct BenchmarkLoggingInitializer;

namespace util {

namespace config {
class ClioConfigDefinition;
}  // namespace config

/**
 * @brief Skips evaluation of expensive argument lists if the given logger is disabled for the required severity level.
 *
 * Note: Currently this introduces potential shadowing (unlikely).
 */
#ifndef COVERAGE_ENABLED
#define LOG(x)                                   \
    if (auto clio_pump__ = x; not clio_pump__) { \
    } else                                       \
        clio_pump__
#else
#define LOG(x) x
#endif

/**
 * @brief Custom severity levels for @ref util::Logger.
 */
enum class Severity {
    TRC,
    DBG,
    NFO,
    WRN,
    ERR,
    FTL,
};

/**
 * @brief A simple thread-safe logger for the channel specified
 * in the constructor.
 *
 * This is cheap to copy and move. Designed to be used as a member variable or
 * otherwise. See @ref LogService::init() for setup of the logging core and
 * severity levels for each channel.
 */
class Logger final {
    std::shared_ptr<spdlog::logger> logger_;

    friend class LogService;  // to expose the Pump interface
    friend struct ::BenchmarkLoggingInitializer;

    /**
     * @brief Helper that pumps data into a log record via `operator<<`.
     */
    class Pump final {
        std::shared_ptr<spdlog::logger> logger_;
        Severity const severity_;
        SourceLocationType const sourceLocation_;
        std::ostringstream stream_;
        bool const enabled_;

    public:
        ~Pump();

        Pump(std::shared_ptr<spdlog::logger> logger, Severity sev, SourceLocationType const& loc);

        Pump(Pump&&) = delete;
        Pump(Pump const&) = delete;
        Pump&
        operator=(Pump const&) = delete;
        Pump&
        operator=(Pump&&) = delete;

        /**
         * @brief Perfectly forwards any incoming data into the underlying stream if data should be logged.
         *
         * @tparam T Type of data to pump
         * @param data The data to pump
         * @return Reference to itself for chaining
         */
        template <typename T>
        [[maybe_unused]] Pump&
        operator<<(T&& data)
        {
            if (enabled_)
                stream_ << std::forward<T>(data);
            return *this;
        }

        /**
         * @return true if logger is enabled; false otherwise
         */
        operator bool() const
        {
            return enabled_;
        }

    private:
        [[nodiscard]] static std::string_view
        prettyPath(SourceLocationType const& loc, size_t maxDepth = 3);
    };

public:
    static constexpr std::array<char const*, 8> kCHANNELS = {
        "General",
        "WebServer",
        "Backend",
        "RPC",
        "ETL",
        "Subscriptions",
        "Performance",
        "Migration",
    };

    /**
     * @brief Construct a new Logger object that produces loglines for the
     * specified channel.
     *
     * See @ref LogService::init() for general setup and configuration of
     * severity levels per channel.
     *
     * @param channel The channel this logger will report into.
     */
    Logger(std::string channel);

    Logger(Logger const&) = default;
    ~Logger() = default;

    Logger(Logger&&) = default;
    Logger&
    operator=(Logger const&) = default;

    Logger&
    operator=(Logger&&) = default;

    /**
     * @brief Interface for logging at Severity::TRC severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    trace(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::DBG severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    debug(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::NFO severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    info(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::WRN severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    warn(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::ERR severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    error(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::FTL severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    fatal(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

private:
    Logger(std::shared_ptr<spdlog::logger> logger);
};

/**
 * @brief A global logging service.
 *
 * Used to initialize and setup the logging core as well as a globally available
 * entrypoint for logging into the `General` channel as well as raising alerts.
 */
class LogService {
    struct Data {
        bool isAsync;
        Severity defaultSeverity;
        std::vector<std::shared_ptr<spdlog::sinks::sink>> allSinks;
    };

    friend class Logger;

private:
    static Data data;

    static std::shared_ptr<spdlog::logger>
    registerLogger(std::string const& channel, Severity severity = data.defaultSeverity);

public:
    LogService() = delete;

    /**
     * @brief Global log core initialization from a @ref config::ClioConfigDefinition
     *
     * @param config The configuration to use
     * @return Void on success, error message on failure
     */
    [[nodiscard]] static std::expected<void, std::string>
    init(config::ClioConfigDefinition const& config);

    /**
     * @brief Shutdown spdlog to guarantee output is not lost
     */
    static void
    shutdown();

    /**
     * @brief Globally accessible General logger at Severity::TRC severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    trace(SourceLocationType const& loc = CURRENT_SRC_LOCATION);

    /**
     * @brief Globally accessible General logger at Severity::DBG severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    debug(SourceLocationType const& loc = CURRENT_SRC_LOCATION);

    /**
     * @brief Globally accessible General logger at Severity::NFO severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    info(SourceLocationType const& loc = CURRENT_SRC_LOCATION);

    /**
     * @brief Globally accessible General logger at Severity::WRN severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    warn(SourceLocationType const& loc = CURRENT_SRC_LOCATION);

    /**
     * @brief Globally accessible General logger at Severity::ERR severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    error(SourceLocationType const& loc = CURRENT_SRC_LOCATION);

    /**
     * @brief Globally accessible General logger at Severity::FTL severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    fatal(SourceLocationType const& loc = CURRENT_SRC_LOCATION);

    /**
     * @brief Whether the LogService is enabled or not
     *
     * @return true if the LogService is enabled, false otherwise
     */
    [[nodiscard]] static bool
    enabled();

private:
    struct FileLoggingParams {
        std::string logDir;

        uint32_t rotationSizeMB;
        uint32_t dirMaxFiles;
    };

    friend struct ::BenchmarkLoggingInitializer;

    [[nodiscard]]
    static std::shared_ptr<spdlog::sinks::sink>
    createFileSink(FileLoggingParams const& params, std::string const& format);
};

};  // namespace util
