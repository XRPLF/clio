#pragma once

#include <ios>
#include <mutex>

#include <gtest/gtest.h>
#include <log/Logger.h>

/**
 * @brief Fixture with LogService support.
 */
class LoggerFixture : public ::testing::Test
{
    /**
     * @brief A simple string buffer that can be used to mock std::cout for
     * console logging.
     */
    class FakeBuffer final : public std::stringbuf
    {
    public:
        std::string
        getStrAndReset()
        {
            auto value = str();
            str("");
            return value;
        }
    };

    FakeBuffer buffer_;
    std::ostream stream_ = std::ostream{&buffer_};

protected:
    // Simulates the `LogService::init(config)` call
    void
    SetUp() override
    {
        static std::once_flag once_;
        std::call_once(once_, [] {
            boost::log::add_common_attributes();
            boost::log::register_simple_formatter_factory<clio::Severity, char>(
                "Severity");
        });

        namespace src = boost::log::sources;
        namespace keywords = boost::log::keywords;
        namespace sinks = boost::log::sinks;
        namespace expr = boost::log::expressions;
        auto core = boost::log::core::get();

        core->remove_all_sinks();
        boost::log::add_console_log(
            stream_, keywords::format = "%Channel%:%Severity% %Message%");
        auto min_severity = expr::channel_severity_filter(
            clio::log_channel, clio::log_severity);
        min_severity["General"] = clio::Severity::DEBUG;
        min_severity["Trace"] = clio::Severity::TRACE;
        core->set_filter(min_severity);
        core->set_logging_enabled(true);
    }

    void
    checkEqual(std::string expected)
    {
        auto value = buffer_.getStrAndReset();
        ASSERT_EQ(value, expected + '\n');
    }

    void
    checkEmpty()
    {
        ASSERT_TRUE(buffer_.getStrAndReset().empty());
    }
};

/**
 * @brief Fixture with LogService support but completely disabled logging.
 *
 * This is meant to be used as a base for other fixtures.
 */
class NoLoggerFixture : public LoggerFixture
{
protected:
    void
    SetUp() override
    {
        LoggerFixture::SetUp();
        boost::log::core::get()->set_logging_enabled(false);
    }
};
