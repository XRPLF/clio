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

#include "data/BackendInterface.h"
#include "util/MockBackend.h"
#include "util/MockCounters.h"
#include "util/MockETLService.h"
#include "util/MockLoadBalancer.h"
#include "util/MockSubscriptionManager.h"
#include "util/log/Logger.h"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/log/core/core.hpp>
#include <boost/log/expressions/predicates/channel_severity_filter.hpp>
#include <boost/log/keywords/format.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <gtest/gtest.h>

#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>

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

protected:
    // Simulates the `util::Logger::init(config)` call
    void
    SetUp() override
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
class NoLoggerFixture : virtual public LoggerFixture {
protected:
    void
    SetUp() override
    {
        LoggerFixture::SetUp();
        boost::log::core::get()->set_logging_enabled(false);
    }
};

/**
 * @brief Fixture with an embedded boost::asio context running on a thread
 *
 * This is meant to be used as a base for other fixtures.
 */
struct AsyncAsioContextTest : virtual public NoLoggerFixture {
    AsyncAsioContextTest()
    {
        work.emplace(ctx);  // make sure ctx does not stop on its own
        runner.emplace([&] { ctx.run(); });
    }

    ~AsyncAsioContextTest() override
    {
        work.reset();
        if (runner->joinable())
            runner->join();
        ctx.stop();
    }

    void
    stop()
    {
        work.reset();
        if (runner->joinable())
            runner->join();
        ctx.stop();
    }

protected:
    boost::asio::io_context ctx;

private:
    std::optional<boost::asio::io_service::work> work;
    std::optional<std::thread> runner;
};

/**
 * @brief Fixture with an embedded boost::asio context that is not running by
 * default but can be progressed on the calling thread
 *
 * Use `run_for(duration)` etc. directly on `ctx`.
 * This is meant to be used as a base for other fixtures.
 */
struct SyncAsioContextTest : virtual public NoLoggerFixture {
    template <typename F>
    void
    runSpawn(F&& f)
    {
        using namespace boost::asio;

        auto called = false;
        spawn(ctx, [&, _ = make_work_guard(ctx)](yield_context yield) {
            f(yield);
            called = true;
        });

        ctx.run();
        ASSERT_TRUE(called);
        ctx.reset();
    }

protected:
    boost::asio::io_context ctx;
};

/**
 * @brief Fixture with a mock backend
 */
template <bool IsNice = true>
struct MockBackendTestBase : virtual public NoLoggerFixture {
    void
    SetUp() override
    {
        NoLoggerFixture::SetUp();
        backend.reset();
        mockBackendPtr = backend;
    }
    void
    TearDown() override
    {
        mockBackendPtr.reset();
        NoLoggerFixture::TearDown();
    }

    class BackendProxy {
        std::shared_ptr<BackendInterface> backend;

    private:
        void
        reset()
        {
            if constexpr (IsNice) {
                backend = std::make_shared<::testing::NiceMock<MockBackend>>(util::Config{});
            } else {
                backend = std::make_shared<MockBackend>(util::Config{});
            }
        }

        friend MockBackendTestBase;

    public:
        auto
        operator->()
        {
            return backend.get();
        }

        operator std::shared_ptr<BackendInterface>()
        {
            return backend;
        }

        operator MockBackend*()
        {
            MockBackend* ret = dynamic_cast<MockBackend*>(backend.get());
            [&] { ASSERT_NE(ret, nullptr); }();
            return ret;
        }
    };

protected:
    BackendProxy backend;
    std::shared_ptr<BackendInterface> mockBackendPtr;  // legacy compat
};

using MockBackendTest = MockBackendTestBase<true>;
using MockBackendTestNaggy = MockBackendTestBase<false>;

/**
 * @brief Fixture with a mock subscription manager
 */
struct MockSubscriptionManagerTest : virtual public NoLoggerFixture {
    void
    SetUp() override
    {
        NoLoggerFixture::SetUp();
        mockSubscriptionManagerPtr = std::make_shared<MockSubscriptionManager>();
    }
    void
    TearDown() override
    {
        mockSubscriptionManagerPtr.reset();
    }

protected:
    std::shared_ptr<MockSubscriptionManager> mockSubscriptionManagerPtr;
};

/**
 * @brief Fixture with a mock etl balancer
 */
struct MockLoadBalancerTest : virtual public NoLoggerFixture {
    void
    SetUp() override
    {
        NoLoggerFixture::SetUp();
        mockLoadBalancerPtr = std::make_shared<MockLoadBalancer>();
    }
    void
    TearDown() override
    {
        mockLoadBalancerPtr.reset();
    }

protected:
    std::shared_ptr<MockLoadBalancer> mockLoadBalancerPtr;
};

/**
 * @brief Fixture with a mock subscription manager
 */
struct MockETLServiceTest : virtual public NoLoggerFixture {
    void
    SetUp() override
    {
        NoLoggerFixture::SetUp();
        mockETLServicePtr = std::make_shared<MockETLService>();
    }
    void
    TearDown() override
    {
        mockETLServicePtr.reset();
    }

protected:
    std::shared_ptr<MockETLService> mockETLServicePtr;
};

/**
 * @brief Fixture with mock counters
 */
struct MockCountersTest : virtual public NoLoggerFixture {
    void
    SetUp() override
    {
        NoLoggerFixture::SetUp();
        mockCountersPtr = std::make_shared<MockCounters>();
    }
    void
    TearDown() override
    {
        mockCountersPtr.reset();
    }

protected:
    std::shared_ptr<MockCounters> mockCountersPtr;
};

/**
 * @brief Fixture with an mock backend and an embedded boost::asio context
 * Handler unittest base class
 */
template <bool IsNice = true>
struct HandlerBaseTestBase : public MockBackendTestBase<IsNice>, public SyncAsioContextTest, public MockETLServiceTest {
protected:
    void
    SetUp() override
    {
        MockBackendTestBase<IsNice>::SetUp();
        SyncAsioContextTest::SetUp();
        MockETLServiceTest::SetUp();
    }

    void
    TearDown() override
    {
        MockETLServiceTest::TearDown();
        SyncAsioContextTest::TearDown();
        MockBackendTestBase<IsNice>::TearDown();
    }
};

using HandlerBaseTest = HandlerBaseTestBase<true>;
using HandlerBaseTestNaggy = HandlerBaseTestBase<false>;
