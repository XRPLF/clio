//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "etl/SystemState.hpp"
#include "etl/impl/AmendmentBlock.hpp"
#include "util/FakeAmendmentBlockAction.hpp"
#include "util/Fixtures.hpp"

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <functional>

using namespace testing;
using namespace etl;

class AmendmentBlockHandlerTest : public NoLoggerFixture {
protected:
    using AmendmentBlockHandlerType = impl::AmendmentBlockHandler<FakeAmendmentBlockAction>;

    boost::asio::io_context ioc_;
};

TEST_F(AmendmentBlockHandlerTest, CallToOnAmendmentBlockSetsStateAndRepeatedlyCallsAction)
{
    std::size_t callCount = 0;
    SystemState state;
    AmendmentBlockHandlerType handler{ioc_, state, std::chrono::nanoseconds{1}, {std::ref(callCount)}};

    EXPECT_FALSE(state.isAmendmentBlocked);
    handler.onAmendmentBlock();
    EXPECT_TRUE(state.isAmendmentBlocked);

    ioc_.run_for(std::chrono::milliseconds{1});
    EXPECT_TRUE(callCount >= 10);
}
