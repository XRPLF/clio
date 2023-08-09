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

#if defined(HAS_SOURCE_LOCATION) && __has_builtin(__builtin_source_location)
// this is used by fully compatible compilers like gcc
#include <source_location>

#elif defined(HAS_EXPERIMENTAL_SOURCE_LOCATION)
// this is used by clang on linux where source_location is still not out of
// experimental headers
#include <experimental/source_location>
#endif

#include <optional>
#include <string>

namespace clio::util {
class Config;

#if defined(HAS_SOURCE_LOCATION) && __has_builtin(__builtin_source_location)
using source_location_t = std::source_location;
#define CURRENT_SRC_LOCATION source_location_t::current()

#elif defined(HAS_EXPERIMENTAL_SOURCE_LOCATION)
using source_location_t = std::experimental::source_location;
#define CURRENT_SRC_LOCATION source_location_t::current()

#else
// A workaround for AppleClang that is lacking source_location atm.
// TODO: remove this workaround when all compilers catch up to c++20
class SourceLocation
{
    std::string_view file_;
    std::size_t line_;

public:
    SourceLocation(std::string_view file, std::size_t line) : file_{file}, line_{line}
    {
    }
    std::string_view
    file_name() const
    {
        return file_;
    }
    std::size_t
    line() const
    {
        return line_;
    }
};
using source_location_t = SourceLocation;
#define CURRENT_SRC_LOCATION source_location_t(__builtin_FILE(), __builtin_LINE())
#endif

/**
 * @brief Custom severity levels for @ref Logger.
 */
enum class Severity {
    TRC,
    DBG,
    NFO,
    WRN,
    ERR,
    FTL,
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
 * @brief A simple thread-safe logger for the channel specified
 * in the constructor.
 *
 * This is cheap to copy and move. Designed to be used as a member variable or
 * otherwise. See @ref LogService::init() for setup of the logging core and
 * severity levels for each channel.
 */
class Logger final
{
    using logger_t = boost::log::sources::severity_channel_logger_mt<Severity, std::string>;
    mutable logger_t logger_;

    friend class LogService;  // to expose the Pump interface

    /**
     * @brief Helper that pumps data into a log record via `operator<<`.
     */
    class Pump final
    {
        using pump_opt_t = std::optional<boost::log::aux::record_pump<logger_t>>;

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
                pump_->stream() << boost::log::add_value("SourceLocation", pretty_path(loc));
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

        /**
         * @brief Custom JSON parser for @ref Severity.
         *
         * @param value The JSON string to parse
         * @return Severity The parsed severity
         * @throws std::runtime_error Thrown if severity is not in the right format
         */
        friend Severity
        tag_invoke(boost::json::value_to_tag<Severity>, boost::json::value const& value);
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
    Logger(std::string channel) : logger_{boost::log::keywords::channel = channel}
    {
    }
    Logger(Logger const&) = default;
    Logger(Logger&&) = default;
    Logger&
    operator=(Logger const&) = default;
    Logger&
    operator=(Logger&&) = default;

    /*! Interface for logging at @ref Severity::TRC severity */
    [[nodiscard]] Pump
    trace(source_location_t const& loc = CURRENT_SRC_LOCATION) const;

    /*! Interface for logging at @ref Severity::DBG severity */
    [[nodiscard]] Pump
    debug(source_location_t const& loc = CURRENT_SRC_LOCATION) const;

    /*! Interface for logging at @ref Severity::INFO severity */
    [[nodiscard]] Pump
    info(source_location_t const& loc = CURRENT_SRC_LOCATION) const;

    /*! Interface for logging at @ref Severity::WRN severity */
    [[nodiscard]] Pump
    warn(source_location_t const& loc = CURRENT_SRC_LOCATION) const;

    /*! Interface for logging at @ref Severity::ERR severity */
    [[nodiscard]] Pump
    error(source_location_t const& loc = CURRENT_SRC_LOCATION) const;

    /*! Interface for logging at @ref Severity::FTL severity */
    [[nodiscard]] Pump
    fatal(source_location_t const& loc = CURRENT_SRC_LOCATION) const;
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

    /*! Globally accesible General logger at @ref Severity::TRC severity */
    [[nodiscard]] static Logger::Pump
    trace(source_location_t const& loc = CURRENT_SRC_LOCATION)
    {
        return general_log_.trace(loc);
    }

    /*! Globally accesible General logger at @ref Severity::DBG severity */
    [[nodiscard]] static Logger::Pump
    debug(source_location_t const& loc = CURRENT_SRC_LOCATION)
    {
        return general_log_.debug(loc);
    }

    /*! Globally accesible General logger at @ref Severity::NFO severity */
    [[nodiscard]] static Logger::Pump
    info(source_location_t const& loc = CURRENT_SRC_LOCATION)
    {
        return general_log_.info(loc);
    }

    /*! Globally accesible General logger at @ref Severity::WRN severity */
    [[nodiscard]] static Logger::Pump
    warn(source_location_t const& loc = CURRENT_SRC_LOCATION)
    {
        return general_log_.warn(loc);
    }

    /*! Globally accesible General logger at @ref Severity::ERR severity */
    [[nodiscard]] static Logger::Pump
    error(source_location_t const& loc = CURRENT_SRC_LOCATION)
    {
        return general_log_.error(loc);
    }

    /*! Globally accesible General logger at @ref Severity::FTL severity */
    [[nodiscard]] static Logger::Pump
    fatal(source_location_t const& loc = CURRENT_SRC_LOCATION)
    {
        return general_log_.fatal(loc);
    }

    /*! Globally accesible Alert logger */
    [[nodiscard]] static Logger::Pump
    alert(source_location_t const& loc = CURRENT_SRC_LOCATION)
    {
        return alert_log_.warn(loc);
    }
};

};  // namespace clio::util
