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

#include "etl/NetworkValidatedLedgers.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/impl/AmendmentBlockHandler.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>

using namespace etl::impl;

struct NetworkValidatedLedgersTests : virtual public ::testing::Test {
protected:
    util::async::CoroExecutionContext ctx_{2};
    std::shared_ptr<etl::NetworkValidatedLedgersInterface> ledgers_ =
        etl::NetworkValidatedLedgers::makeValidatedLedgers();
};

TEST_F(NetworkValidatedLedgersTests, WaitUntilValidatedByNetworkWithoutTimeout)
{
    auto awaitable = ctx_.execute([this] { return ledgers_->waitUntilValidatedByNetwork(123u); });

    ledgers_->push(122u);
    ledgers_->push(123u);

    EXPECT_TRUE(awaitable.get().value());
}

TEST_F(NetworkValidatedLedgersTests, WaitUntilValidatedByNetworkWithTimeout)
{
    static constexpr auto kTIMEOUT_MILLIS = 10u;
    auto awaitable = ctx_.execute([this] {
        return ledgers_->waitUntilValidatedByNetwork(123u, kTIMEOUT_MILLIS);
    });

    ledgers_->push(122u);

    EXPECT_FALSE(awaitable.get().value());
}

TEST_F(NetworkValidatedLedgersTests, GetMostRecent)
{
    ledgers_->push(122u);
    ledgers_->push(123u);

    auto awaitable = ctx_.execute([this] { return ledgers_->getMostRecent(); });

    EXPECT_EQ(awaitable.get().value(), 123u);

    ledgers_->push(124u);
    EXPECT_EQ(ledgers_->getMostRecent(), 124u);
}

TEST_F(NetworkValidatedLedgersTests, SubscribersGetNotifiedWhileConnectionIsAlive)
{
    testing::StrictMock<testing::MockFunction<void(uint32_t)>> actionMock1, actionMock2;
    EXPECT_CALL(actionMock1, Call).Times(2);
    EXPECT_CALL(actionMock2, Call).Times(2);

    {
        auto connection1 = ledgers_->subscribe(actionMock1.AsStdFunction());
        auto connection2 = ledgers_->subscribe(actionMock2.AsStdFunction());

        ledgers_->push(123u);
        ledgers_->push(124u);
    }

    ledgers_->push(125u);
    ledgers_->push(126u);
}
