//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/SignalsHandler.hpp"
#include "util/config/Config.hpp"

#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using namespace util;
using testing::MockFunction;
using testing::StrictMock;

struct SignalsHandlerTests : public ::testing::Test {
    StrictMock<MockFunction<void(std::string)>> forceExitHandler_;
    StrictMock<MockFunction<void()>> stopHandler_;
    StrictMock<MockFunction<void()>> anotherStopHandler_;

    SignalsHandler handler_{
        util::Config{boost::json::value{{"graceful_period", 0.001}}},
        forceExitHandler_.AsStdFunction()
    };
};

TEST_F(SignalsHandlerTests, no_signal)
{
    handler_.subscribeToStop(stopHandler_.AsStdFunction());
    handler_.subscribeToStop(anotherStopHandler_.AsStdFunction());
}

// no signal test
// one signal test
// two signals test
// priority test
