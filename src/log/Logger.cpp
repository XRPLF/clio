#include <config/Config.h>
#include <log/Logger.h>

#include <algorithm>
#include <array>
#include <filesystem>

namespace clio {

Logger LogService::general_log_ = Logger{"General"};
Logger LogService::alert_log_ = Logger{"Alert"};

std::ostream&
operator<<(std::ostream& stream, Severity sev)
{
    static constexpr std::array<const char*, 6> labels = {
        "TRC",
        "DBG",
        "NFO",
        "WRN",
        "ERR",
        "FTL",
    };

    return stream << labels.at(static_cast<int>(sev));
}

Severity
tag_invoke(boost::json::value_to_tag<Severity>, boost::json::value const& value)
{
    if (not value.is_string())
        throw std::runtime_error("`log_level` must be a string");
    auto const& logLevel = value.as_string();

    if (boost::iequals(logLevel, "trace"))
        return Severity::TRACE;
    else if (boost::iequals(logLevel, "debug"))
        return Severity::DEBUG;
    else if (boost::iequals(logLevel, "info"))
        return Severity::INFO;
    else if (
        boost::iequals(logLevel, "warning") || boost::iequals(logLevel, "warn"))
        return Severity::WARNING;
    else if (boost::iequals(logLevel, "error"))
        return Severity::ERROR;
    else if (boost::iequals(logLevel, "fatal"))
        return Severity::FATAL;
    else
        throw std::runtime_error(
            "Could not parse `log_level`: expected `trace`, `debug`, `info`, "
            "`warning`, `error` or `fatal`");
}

void
LogService::init(Config const& config)
{
    namespace src = boost::log::sources;
    namespace keywords = boost::log::keywords;
    namespace sinks = boost::log::sinks;

    boost::log::add_common_attributes();
    boost::log::register_simple_formatter_factory<Severity, char>("Severity");
    auto const defaultFormat =
        "%TimeStamp% (%SourceLocation%) [%ThreadID%] %Channel%:%Severity% "
        "%Message%";
    std::string format =
        config.valueOr<std::string>("log_format", defaultFormat);

    if (config.valueOr("log_to_console", false))
    {
        boost::log::add_console_log(std::cout, keywords::format = format);
    }

    auto logDir = config.maybeValue<std::string>("log_directory");
    if (logDir)
    {
        boost::filesystem::path dirPath{logDir.value()};
        if (!boost::filesystem::exists(dirPath))
            boost::filesystem::create_directories(dirPath);
        auto const rotationSize =
            config.valueOr<uint64_t>("log_rotation_size", 2048u) * 1024u *
            1024u;
        auto const rotationPeriod =
            config.valueOr<uint32_t>("log_rotation_hour_interval", 12u);
        auto const dirSize =
            config.valueOr<uint64_t>("log_directory_max_size", 50u * 1024u) *
            1024u * 1024u;
        auto fileSink = boost::log::add_file_log(
            keywords::file_name = dirPath / "clio.log",
            keywords::target_file_name = dirPath / "clio_%Y-%m-%d_%H-%M-%S.log",
            keywords::auto_flush = true,
            keywords::format = format,
            keywords::open_mode = std::ios_base::app,
            keywords::rotation_size = rotationSize,
            keywords::time_based_rotation =
                sinks::file::rotation_at_time_interval(
                    boost::posix_time::hours(rotationPeriod)));
        fileSink->locked_backend()->set_file_collector(
            sinks::file::make_collector(
                keywords::target = dirPath, keywords::max_size = dirSize));
        fileSink->locked_backend()->scan_for_files();
    }

    // get default severity, can be overridden per channel using
    // the `log_channels` array
    auto defaultSeverity =
        config.valueOr<Severity>("log_level", Severity::INFO);
    static constexpr std::array<const char*, 7> channels = {
        "General",
        "WebServer",
        "Backend",
        "RPC",
        "ETL",
        "Subscriptions",
        "Performance",
    };

    auto core = boost::log::core::get();
    auto min_severity = boost::log::expressions::channel_severity_filter(
        log_channel, log_severity);

    for (auto const& channel : channels)
        min_severity[channel] = defaultSeverity;
    min_severity["Alert"] =
        Severity::WARNING;  // Channel for alerts, always warning severity

    for (auto const overrides = config.arrayOr("log_channels", {});
         auto const& cfg : overrides)
    {
        auto name = cfg.valueOrThrow<std::string>(
            "channel", "Channel name is required");
        if (not std::count(std::begin(channels), std::end(channels), name))
            throw std::runtime_error(
                "Can't override settings for log channel " + name +
                ": invalid channel");

        min_severity[name] =
            cfg.valueOr<Severity>("log_level", defaultSeverity);
    }

    core->set_filter(min_severity);
    LogService::info() << "Default log level = " << defaultSeverity;
}

Logger::Pump
Logger::trace(source_location_t const loc) const
{
    return {logger_, Severity::TRACE, loc};
};
Logger::Pump
Logger::debug(source_location_t const loc) const
{
    return {logger_, Severity::DEBUG, loc};
};
Logger::Pump
Logger::info(source_location_t const loc) const
{
    return {logger_, Severity::INFO, loc};
};
Logger::Pump
Logger::warn(source_location_t const loc) const
{
    return {logger_, Severity::WARNING, loc};
};
Logger::Pump
Logger::error(source_location_t const loc) const
{
    return {logger_, Severity::ERROR, loc};
};
Logger::Pump
Logger::fatal(source_location_t const loc) const
{
    return {logger_, Severity::FATAL, loc};
};

std::string
Logger::Pump::pretty_path(source_location_t const& loc, size_t max_depth) const
{
    auto const file_path = std::string{loc.file_name()};
    auto idx = file_path.size();
    while (max_depth-- > 0)
    {
        idx = file_path.rfind('/', idx - 1);
        if (idx == std::string::npos || idx == 0)
            break;
    }
    return file_path.substr(idx == std::string::npos ? 0 : idx + 1) + ':' +
        std::to_string(loc.line());
}

}  // namespace clio
