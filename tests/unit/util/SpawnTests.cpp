#include "util/Spawn.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <gtest/gtest.h>

#include <stdexcept>

TEST(SpawnTest, SpawnOnIoContext)
{
    EXPECT_ANY_THROW([] {
        boost::asio::io_context io;
        util::spawn(io, [](boost::asio::yield_context) {
            throw std::runtime_error("Test exception in coroutine");
        });

        io.run();
    }());
}

TEST(SpawnTest, SpawnOnStrand)
{
    EXPECT_ANY_THROW([] {
        boost::asio::io_context io;
        auto str = boost::asio::make_strand(io);
        util::spawn(str, [](boost::asio::yield_context) {
            throw std::runtime_error("Test exception in coroutine");
        });

        io.run();
    }());
}

TEST(SpawnTest, SpawnOnCoroutine)
{
    EXPECT_ANY_THROW([] {
        boost::asio::io_context io;
        util::spawn(io, [](boost::asio::yield_context yield) {
            util::spawn(yield, [](boost::asio::yield_context) {
                throw std::runtime_error("Test exception in coroutine");
            });
        });

        io.run();
    }());
}
