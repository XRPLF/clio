#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <source_location>
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
class LoggerFixture;
struct LogServiceInitTests;
struct LogFileRotationTests;

namespace util {

namespace impl {
class OnAssert;
}  // namespace impl

namespace config {
class ClioConfigDefinition;
}  // namespace config

/**
 * @brief Skips evaluation of expensive argument lists if the given logger is disabled for the
 * required severity level.
 *
 * Note: Currently this introduces potential shadowing (unlikely).
 */
#ifndef COVERAGE_ENABLED
#define LOG(x)                                 \
    if (auto clio_pump__ = x; not clio_pump__) \
        ;                                      \
    else                                       \
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
class Logger {
    std::shared_ptr<spdlog::logger> logger_;

    friend class LogService;  // to expose the Pump interface
    friend struct ::BenchmarkLoggingInitializer;

    /**
     * @brief Helper that pumps data into a log record via `operator<<`.
     */
    class Pump final {
        std::shared_ptr<spdlog::logger> logger_;
        Severity const severity_;
        std::source_location const sourceLocation_;
        std::ostringstream stream_;
        bool const enabled_;

    public:
        ~Pump();

        Pump(std::shared_ptr<spdlog::logger> logger, Severity sev, std::source_location const& loc);

        Pump(Pump&&) = delete;
        Pump(Pump const&) = delete;
        Pump&
        operator=(Pump const&) = delete;
        Pump&
        operator=(Pump&&) = delete;

        /**
         * @brief Perfectly forwards any incoming data into the underlying stream if data should be
         * logged.
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
    };

public:
    static constexpr std::array<std::string_view, 8> kChannels = {
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
    Logger(std::string_view const channel);

    Logger(Logger const&) = default;
    ~Logger();

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
    trace(std::source_location const& loc = std::source_location::current()) const;

    /**
     * @brief Interface for logging at Severity::DBG severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    debug(std::source_location const& loc = std::source_location::current()) const;

    /**
     * @brief Interface for logging at Severity::NFO severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    info(std::source_location const& loc = std::source_location::current()) const;

    /**
     * @brief Interface for logging at Severity::WRN severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    warn(std::source_location const& loc = std::source_location::current()) const;

    /**
     * @brief Interface for logging at Severity::ERR severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    error(std::source_location const& loc = std::source_location::current()) const;

    /**
     * @brief Interface for logging at Severity::FTL severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    fatal(std::source_location const& loc = std::source_location::current()) const;

private:
    Logger(std::shared_ptr<spdlog::logger> logger);
};

/**
 * @brief Base state management class for the logging service.
 *
 * This class manages the global state and core functionality for the logging system,
 * including initialization, sink management, and logger registration.
 */
class LogServiceState {
protected:
    friend struct ::LogServiceInitTests;
    friend class ::LoggerFixture;
    friend struct ::LogFileRotationTests;
    friend class Logger;
    friend class ::util::impl::OnAssert;

    /**
     * @brief Initialize the logging core with specified parameters.
     *
     * @param isAsync Whether logging should be asynchronous
     * @param defaultSeverity The default severity level for new loggers
     * @param sinks Vector of spdlog sinks to use for output
     */
    static void
    init(
        bool isAsync,
        Severity defaultSeverity,
        std::vector<std::shared_ptr<spdlog::sinks::sink>> const& sinks
    );

    /**
     * @brief Whether the LogService is initialized or not
     *
     * @return true if the LogService is initialized
     */
    [[nodiscard]] static bool
    initialized();

    /**
     * @brief Whether the LogService has any sink. If there is no sink, logger will not log messages
     * anywhere.
     *
     * @return true if the LogService has at least one sink
     */
    [[nodiscard]] static bool
    hasSinks();

    /**
     * @brief Reset the logging service to uninitialized state.
     */
    static void
    reset();

    /**
     * @brief Replace the current sinks with a new set of sinks.
     *
     * @param sinks Vector of new spdlog sinks to replace the current ones
     */
    static void
    replaceSinks(std::vector<std::shared_ptr<spdlog::sinks::sink>> const& sinks);

    /**
     * @brief Register a new logger for the specified channel.
     *
     * Creates and registers a new spdlog logger instance for the given channel
     * with the specified or default severity level.
     *
     * @param channel The name of the logging channel
     * @param severity Optional severity level override; uses default if not specified
     * @return Shared pointer to the registered spdlog logger
     */
    static std::shared_ptr<spdlog::logger>
    registerLogger(std::string_view channel, std::optional<Severity> severity = std::nullopt);

protected:
    static bool isAsync_;              // NOLINT(readability-identifier-naming)
    static Severity defaultSeverity_;  // NOLINT(readability-identifier-naming)
    static std::vector<std::shared_ptr<spdlog::sinks::sink>>
        sinks_;                // NOLINT(readability-identifier-naming)
    static bool initialized_;  // NOLINT(readability-identifier-naming)
};

/**
 * @brief A global logging service.
 *
 * Used to initialize and setup the logging core as well as a globally available
 * entrypoint for logging into the `General` channel as well as raising alerts.
 */
class LogService : public LogServiceState {
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
    trace(std::source_location const& loc = std::source_location::current());

    /**
     * @brief Globally accessible General logger at Severity::DBG severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    debug(std::source_location const& loc = std::source_location::current());

    /**
     * @brief Globally accessible General logger at Severity::NFO severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    info(std::source_location const& loc = std::source_location::current());

    /**
     * @brief Globally accessible General logger at Severity::WRN severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    warn(std::source_location const& loc = std::source_location::current());

    /**
     * @brief Globally accessible General logger at Severity::ERR severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    error(std::source_location const& loc = std::source_location::current());

    /**
     * @brief Globally accessible General logger at Severity::FTL severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    fatal(std::source_location const& loc = std::source_location::current());

private:
    /**
     * @brief Parses the sinks from a @ref config::ClioConfigDefinition
     *
     * @param config The configuration to parse sinks from
     * @return A vector of sinks on success, error message on failure
     */
    [[nodiscard]] static std::
        expected<std::vector<std::shared_ptr<spdlog::sinks::sink>>, std::string>
        getSinks(config::ClioConfigDefinition const& config);

    struct RotationParams {
        uint32_t sizeMB;
        uint32_t maxFiles;
    };

    struct FileLoggingParams {
        std::string logDir;
        std::optional<RotationParams> rotation;  ///< nullopt when rotation is disabled
    };

    friend struct ::BenchmarkLoggingInitializer;

    [[nodiscard]]
    static std::shared_ptr<spdlog::sinks::sink>
    createFileSink(FileLoggingParams const& params, std::string const& format);
};

};  // namespace util
