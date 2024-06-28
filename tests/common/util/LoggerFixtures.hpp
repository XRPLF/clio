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

#include "util/log/Logger.hpp"

#include <boost/log/core/core.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <gtest/gtest.h>

#include <mutex>
#include <ostream>
#include <sstream>
#include <string>

/**
 * @brief Fixture with util::Logger support.
 */
class LoggerFixture : virtual public ::testing::Test {
    /**
     * @brief A simple string buffer that can be used to mock std::cout for
     * console logging.
     */
    class FakeBuffer final : public std::stringbuf {
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

public:
    // Simulates the `util::Logger::init(config)` call
    LoggerFixture()
    {
        static std::once_flag once_;
        std::call_once(once_, [] {
            boost::log::add_common_attributes();
            boost::log::register_simple_formatter_factory<util::Severity, char>("Severity");
        });

        namespace keywords = boost::log::keywords;
        namespace expr = boost::log::expressions;
        auto core = boost::log::core::get();

        core->remove_all_sinks();
        boost::log::add_console_log(stream_, keywords::format = "%Channel%:%Severity% %Message%");
        auto min_severity = expr::channel_severity_filter(util::log_channel, util::log_severity);
        min_severity["General"] = util::Severity::DBG;
        min_severity["Trace"] = util::Severity::TRC;
        core->set_filter(min_severity);
        core->set_logging_enabled(true);
    }

protected:
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
 * @brief Fixture with util::Logger support but completely disabled logging.
 *
 * This is meant to be used as a base for other fixtures.
 */
struct NoLoggerFixture : virtual LoggerFixture {
    NoLoggerFixture()
    {
        boost::log::core::get()->set_logging_enabled(false);
    }
};
