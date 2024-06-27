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

#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockETLServiceTestFixture.hpp"
#include "util/MockPrometheus.hpp"

/**
 * @brief Fixture with an mock backend and an embedded boost::asio context.
 *
 * Use as a handler unittest base fixture thru either @see HandlerBaseTest, @see HandlerBaseTestNaggy or @see
 * HandlerBaseTestStrict.
 */
template <template <typename> typename MockType = ::testing::NiceMock>
struct HandlerBaseTestBase : util::prometheus::WithPrometheus,
                             MockBackendTestBase<MockType>,
                             SyncAsioContextTest,
                             MockETLServiceTestBase<MockType> {
protected:
    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
        MockETLServiceTestBase<MockType>::SetUp();
    }

    void
    TearDown() override
    {
        MockETLServiceTestBase<MockType>::TearDown();
        SyncAsioContextTest::TearDown();
    }
};

/**
 * @brief Fixture with a "nice" backend mock and an embedded boost::asio context.
 *
 * Use @see HandlerBaseTest during development to get unset call expectation warnings from the backend mock.
 * Once the test is ready and you are happy you can switch to this fixture to mute the warnings.
 *
 * @see BackendBaseTest for more details on the injected backend mock.
 */
using HandlerBaseTest = HandlerBaseTestBase<::testing::NiceMock>;

/**
 * @brief Fixture with a "naggy" backend mock and an embedded boost::asio context.
 *
 * Use this during development to get unset call expectation warnings from the backend mock.
 */
using HandlerBaseTestNaggy = HandlerBaseTestBase<::testing::NaggyMock>;

/**
 * @brief Fixture with a "strict" backend mock and an embedded boost::asio context.
 */
using HandlerBaseTestStrict = HandlerBaseTestBase<::testing::StrictMock>;
