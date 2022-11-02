#ifndef RIPPLE_UTIL_LOGGER_H
#define RIPPLE_UTIL_LOGGER_H

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/log/core/core.hpp>
#include <boost/log/expressions/predicates/channel_severity_filter.hpp>
#include <boost/log/sinks/unlocked_frontend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>

// Note: clang still does not provide non-experimental support, gcc does
// TODO: start using <source_location> once clang catches up on c++20
#include <experimental/source_location>

#include <optional>
#include <string>

namespace clio {

class Config;
using source_location_t = std::experimental::source_location;

/**
 * @brief Custom severity levels for @ref Logger.
 */
enum class Severity {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
};

BOOST_LOG_ATTRIBUTE_KEYWORD(log_severity, "Severity", Severity);
BOOST_LOG_ATTRIBUTE_KEYWORD(log_channel, "Channel", std::string);

/**
 * @brief Custom labels for @ref Severity in log output.
 *
 * @param stream std::ostream The output stream
 * @param sev Severity The severity to output to the ostream
 * @return std::ostream& The same ostream we were given
 */
std::ostream&
operator<<(std::ostream& stream, Severity sev);

/**
 * @brief Custom JSON parser for @ref Severity.
 *
 * @param value The JSON string to parse
 * @return Severity The parsed severity
 * @throws std::runtime_error Thrown if severity is not in the right format
 */
Severity
tag_invoke(
    boost::json::value_to_tag<Severity>,
    boost::json::value const& value);

/**
 * @brief A simple thread-safe logger for the channel specified
 * in the constructor.
 *
 * This is cheap to copy and move. Designed to be used as a member variable or
 * otherwise. See @ref LogService::init() for setup of the logging core and
 * severity levels for each channel.
 */
class Logger final
{
    using logger_t =
        boost::log::sources::severity_channel_logger_mt<Severity, std::string>;
    mutable logger_t logger_;

    friend class LogService;  // to expose the Pump interface

    /**
     * @brief Helper that pumps data into a log record via `operator<<`.
     */
    class Pump final
    {
        using pump_opt_t =
            std::optional<boost::log::aux::record_pump<logger_t>>;

        boost::log::record rec_;
        pump_opt_t pump_ = std::nullopt;

    public:
        ~Pump() = default;
        Pump(logger_t& logger, Severity sev, source_location_t const& loc)
            : rec_{logger.open_record(boost::log::keywords::severity = sev)}
        {
            if (rec_)
            {
                pump_.emplace(boost::log::aux::make_record_pump(logger, rec_));
                pump_->stream() << boost::log::add_value(
                    "SourceLocation", pretty_path(loc));
            }
        }

        Pump(Pump&&) = delete;
        Pump(Pump const&) = delete;
        Pump&
        operator=(Pump const&) = delete;
        Pump&
        operator=(Pump&&) = delete;

        /**
         * @brief Perfectly forwards any incoming data into the underlying
         * boost::log pump if the pump is available. nop otherwise.
         *
         * @tparam T Type of data to pump
         * @param data The data to pump
         * @return Pump& Reference to itself for chaining
         */
        template <typename T>
        [[maybe_unused]] Pump&
        operator<<(T&& data)
        {
            if (pump_)
                pump_->stream() << std::forward<T>(data);
            return *this;
        }

    private:
        [[nodiscard]] std::string
        pretty_path(source_location_t const& loc, size_t max_depth = 3) const;
    };

public:
    ~Logger() = default;
    /**
     * @brief Construct a new Logger object that produces loglines for the
     * specified channel.
     *
     * See @ref LogService::init() for general setup and configuration of
     * severity levels per channel.
     *
     * @param channel The channel this logger will report into.
     */
    Logger(std::string channel)
        : logger_{boost::log::keywords::channel = channel}
    {
    }
    Logger(Logger const&) = default;
    Logger(Logger&&) = default;
    Logger&
    operator=(Logger const&) = default;
    Logger&
    operator=(Logger&&) = default;

    /*! Interface for logging at @ref Severity::TRACE severity */
    [[nodiscard]] Pump
    trace(source_location_t const loc = source_location_t::current()) const;

    /*! Interface for logging at @ref Severity::DEBUG severity */
    [[nodiscard]] Pump
    debug(source_location_t const loc = source_location_t::current()) const;

    /*! Interface for logging at @ref Severity::INFO severity */
    [[nodiscard]] Pump
    info(source_location_t const loc = source_location_t::current()) const;

    /*! Interface for logging at @ref Severity::WARNING severity */
    [[nodiscard]] Pump
    warn(source_location_t const loc = source_location_t::current()) const;

    /*! Interface for logging at @ref Severity::ERROR severity */
    [[nodiscard]] Pump
    error(source_location_t const loc = source_location_t::current()) const;

    /*! Interface for logging at @ref Severity::FATAL severity */
    [[nodiscard]] Pump
    fatal(source_location_t const loc = source_location_t::current()) const;
};

/**
 * @brief A global logging service.
 *
 * Used to initialize and setup the logging core as well as a globally available
 * entrypoint for logging into the `General` channel as well as raising alerts.
 */
class LogService
{
    static Logger general_log_; /*! Global logger for General channel */
    static Logger alert_log_;   /*! Global logger for Alerts channel */

public:
    LogService() = delete;

    /**
     * @brief Global log core initialization from a @ref Config
     */
    static void
    init(Config const& config);

    /*! Globally accesible General logger at @ref Severity::TRACE severity */
    [[nodiscard]] static Logger::Pump
    trace(source_location_t const loc = source_location_t::current())
    {
        return general_log_.trace(loc);
    }

    /*! Globally accesible General logger at @ref Severity::TRACE severity */
    [[nodiscard]] static Logger::Pump
    debug(source_location_t const loc = source_location_t::current())
    {
        return general_log_.debug(loc);
    }

    /*! Globally accesible General logger at @ref Severity::INFO severity */
    [[nodiscard]] static Logger::Pump
    info(source_location_t const loc = source_location_t::current())
    {
        return general_log_.info(loc);
    }

    /*! Globally accesible General logger at @ref Severity::WARNING severity */
    [[nodiscard]] static Logger::Pump
    warn(source_location_t const loc = source_location_t::current())
    {
        return general_log_.warn(loc);
    }

    /*! Globally accesible General logger at @ref Severity::ERROR severity */
    [[nodiscard]] static Logger::Pump
    error(source_location_t const loc = source_location_t::current())
    {
        return general_log_.error(loc);
    }

    /*! Globally accesible General logger at @ref Severity::FATAL severity */
    [[nodiscard]] static Logger::Pump
    fatal(source_location_t const loc = source_location_t::current())
    {
        return general_log_.fatal(loc);
    }

    /*! Globally accesible Alert logger */
    [[nodiscard]] static Logger::Pump
    alert(source_location_t const loc = source_location_t::current())
    {
        return alert_log_.warn(loc);
    }
};

};  // namespace clio

#endif  // RIPPLE_UTIL_LOGGER_H
