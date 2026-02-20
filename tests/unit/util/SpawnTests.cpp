//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
