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

#pragma once

#include "util/Fixtures.hpp"
#include "util/MockETLService.hpp"
#include "util/MockLoadBalancer.hpp"

#include <gmock/gmock.h>

/**
 * @brief Fixture with a mock etl service
 */
template <template <typename> typename MockType = ::testing::NiceMock>
struct MockETLServiceTestBase : virtual public NoLoggerFixture {
    using Mock = MockType<MockETLService>;

protected:
    std::shared_ptr<Mock> mockETLServicePtr = std::make_shared<Mock>();
};

/**
 * @brief Fixture with a "nice" ETLService mock.
 *
 * Use @see MockETLServiceTestNaggy during development to get unset call expectation warnings from the embeded mock.
 * Once the test is ready and you are happy you can switch to this fixture to mute the warnings.
 */
using MockETLServiceTest = MockETLServiceTestBase<::testing::NiceMock>;

/**
 * @brief Fixture with a "naggy" ETLService mock.
 *
 * Use this during development to get unset call expectation warnings from the embedded mock.
 */
using MockETLServiceTestNaggy = MockETLServiceTestBase<::testing::NaggyMock>;

/**
 * @brief Fixture with a "strict" ETLService mock.
 */
using MockETLServiceTestStrict = MockETLServiceTestBase<::testing::StrictMock>;

/**
 * @brief Fixture with a mock etl balancer
 */
struct MockLoadBalancerTest : virtual public NoLoggerFixture {
protected:
    std::shared_ptr<MockLoadBalancer> mockLoadBalancerPtr = std::make_shared<MockLoadBalancer>();
};
