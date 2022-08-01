#include <algorithm>
#include <clio/backend/BackendFactory.h>
#include <clio/backend/DBHelpers.h>
#include <clio/rpc/RPCHelpers.h>
#include <gtest/gtest.h>

#include <test/env/env.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

TYPED_TEST_SUITE(Clio, cfgMOCK);

TYPED_TEST(Clio, channelVerifyForwards)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.minSequence = 1;
            range.maxSequence = 63116314;

            boost::json::object request = {{"method", "channel_verify"}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);
            ASSERT_TRUE(RPC::shouldForwardToRippled(*context));

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}
